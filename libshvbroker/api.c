#include "api.h"
#include <shv/rpchandler_impl.h>

#include "api_broker_method.gperf.h"
#include "api_current_client_method.gperf.h"

void rpcbroker_api_ls(struct clientctx *c, struct rpchandler_ls *ctx) {
	if (ctx->path[0] == '\0') {
		rpchandler_ls_result_const(ctx, ".broker");
	} else if (!strcmp(ctx->path, ".broker")) {
		rpchandler_ls_result_const(ctx, "currentClient");
		rpchandler_ls_result_const(ctx, "client");
		return; /* No one can mount here so no reason to check */
	} else if (!strcmp(ctx->path, ".broker/client")) {
		broker_lock(c->broker);
		for_cid(c->broker) {
			if (cid_valid(c->broker, cid) && c->broker->clients[cid].role)
				rpchandler_ls_result_fmt(ctx, "%d", cid);
		}
		broker_unlock(c->broker);
		return; /* No one can mount here so no reason to check */
	}

	broker_lock(c->broker);
	for (size_t i = 0; i < c->broker->mounts_cnt; i++)
		if (is_path_prefix(c->broker->mounts[i].path, ctx->path)) {
			const char *s = c->broker->mounts[i].path + strlen(ctx->path) +
				(*ctx->path != '\0' ? 1 : 0);
			const char *e = strchrnul(s, '/');
			/* Note that s != e because in such case we would propagate this
			 * ls request instead of handling it in the broker.
			 */
			assert(s != e);
			rpchandler_ls_result_fmt(ctx, "%.*s", (int)(e - s), s);
		}
	broker_unlock(c->broker);
}

bool rpcbroker_api_dir(struct rpchandler_dir *ctx) {
	if (!strcmp(ctx->path, ".broker")) {
		if (ctx->name) { /* Faster match against gperf */
			if (gperf_api_broker_method(ctx->name, strlen(ctx->name)))
				rpchandler_dir_exists(ctx);
			return true;
		}
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "name",
				.result = "String",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "clientInfo",
				.param = "Int",
				.result = "ClientInfo",
				.access = RPCACCESS_SUPER_SERVICE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "mountedClientInfo",
				.param = "String",
				.result = "ClientInfo",
				.access = RPCACCESS_SUPER_SERVICE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "clients",
				.result = "List[Int]",
				.access = RPCACCESS_SUPER_SERVICE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "mounts",
				.result = "List[String]",
				.access = RPCACCESS_SUPER_SERVICE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "disconnectClient",
				.param = "Int",
				.access = RPCACCESS_SUPER_SERVICE,
			});
	} else if (!strcmp(ctx->path, ".broker/currentClient")) {
		if (ctx->name) { /* Faster match against gperf */
			if (gperf_api_current_client_method(ctx->name, strlen(ctx->name)))
				rpchandler_dir_exists(ctx);
			return true;
		}
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "info",
				.result = "ClientInfo",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "subscribe",
				.param = "String",
				.result = "Bool",
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "unsubscribe",
				.param = "String",
				.result = "Bool",
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "subscriptions",
				.result = "List[String]",
				.access = RPCACCESS_BROWSE,
			});
	}
	return false;
}


static void pack_subscriptions(cp_pack_t pack, struct clientctx *client) {
	cp_pack_map_begin(pack);
	int cid = client - client->broker->clients;
	time_t now = get_time();
	for (size_t i = 0; i < client->broker->subscriptions_cnt; i++)
		if (nbool(client->broker->subscriptions[i].clients, cid)) {
			cp_pack_str(pack, client->broker->subscriptions[i].ri);
			size_t y = 0;
			while (y < client->ttlsubs_cnt &&
				client->ttlsubs[y].ri != client->broker->subscriptions[i].ri)
				y++;
			if (y < client->ttlsubs_cnt)
				cp_pack_int(pack, client->ttlsubs[y].ttl - now);
			else
				cp_pack_null(pack);
		}
	cp_pack_container_end(pack);
}

static void send_client_info(
	struct rpcbroker *broker, int cid, struct rpchandler_msg *ctx) {
	struct clientctx *client = &broker->clients[cid];
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_map_begin(pack);
	cp_pack_str(pack, "clientId");
	cp_pack_int(pack, cid);
	if (client->role) {
		if (client->username) {
			cp_pack_str(pack, "userName");
			cp_pack_str(pack, client->username);
		}
		if (client->role->mount_point) {
			cp_pack_str(pack, "mountPoint");
			cp_pack_str(pack, client->role->mount_point);
		}
		if (client->role->name) {
			cp_pack_str(pack, "role");
			cp_pack_str(pack, client->role->name);
		}
		cp_pack_str(pack, "subscriptions");
		pack_subscriptions(pack, client);
	}
	cp_pack_container_end(pack);
	rpchandler_msg_send_response(ctx, pack);
}

static inline bool rpc_msg_request_broker(
	struct clientctx *c, struct rpchandler_msg *ctx) {
	const struct gperf_api_broker_method_match *match =
		gperf_api_broker_method(ctx->meta.method, strlen(ctx->meta.method));
	if (match != NULL)
		switch (match->method) {
			case M_NAME: {
				bool has_param = rpcmsg_has_value(ctx->item);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (has_param) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Must be 'null'");
					return true;
				}
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				broker_lock(c->broker);
				cp_pack_str(pack, c->broker->name);
				broker_unlock(c->broker);
				rpchandler_msg_send_response(ctx, pack);
				return true;
			}
			case M_CLIENT_INFO: {
				if (ctx->meta.access < RPCACCESS_SUPER_SERVICE)
					break;
				int cid;
				bool valid = cp_unpack_int(ctx->unpack, ctx->item, cid);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (!valid) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Expected Int");
					return true;
				}
				broker_lock(c->broker);
				if (cid_valid(c->broker, cid))
					send_client_info(c->broker, cid, ctx);
				else
					rpchandler_msg_send_response_void(ctx);
				broker_unlock(c->broker);
				return true;
			}
			case M_MOUNTED_CLIENT_INFO: {
				if (ctx->meta.access < RPCACCESS_SUPER_SERVICE)
					break;
				char *path = cp_unpack_strdupo(
					ctx->unpack, ctx->item, rpchandler_obstack(ctx));
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (path == NULL) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Expected String");
					return true;
				}
				broker_lock(c->broker);
				struct clientctx *client = mounted_client(c->broker, path, NULL);
				if (client)
					send_client_info(c->broker, client - c->broker->clients, ctx);
				else
					rpchandler_msg_send_response_void(ctx);
				broker_unlock(c->broker);
				return true;
			}
			case M_CLIENTS: {
				if (ctx->meta.access < RPCACCESS_SUPER_SERVICE)
					break;
				bool has_param = rpcmsg_has_value(ctx->item);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (has_param) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Must be 'null'");
					return true;
				}
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_list_begin(pack);
				broker_lock(c->broker);
				for (int i = 0; i < c->broker->clients_cnt; i++)
					if (cid_valid(c->broker, i))
						cp_pack_int(pack, i);
				broker_unlock(c->broker);
				cp_pack_container_end(pack);
				rpchandler_msg_send_response(ctx, pack);
				return true;
			}
			case M_MOUNTS: {
				if (ctx->meta.access < RPCACCESS_SUPER_SERVICE)
					break;
				bool has_param = rpcmsg_has_value(ctx->item);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (has_param) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Must be 'null'");
					return true;
				}
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_list_begin(pack);
				broker_lock(c->broker);
				for (size_t i = 0; i < c->broker->mounts_cnt; i++)
					cp_pack_str(pack, c->broker->mounts[i].path);
				broker_unlock(c->broker);
				cp_pack_container_end(pack);
				rpchandler_msg_send_response(ctx, pack);
				return true;
			}
			case M_DISCONNECT_CLIENT: {
				if (ctx->meta.access < RPCACCESS_SUPER_SERVICE)
					break;
				int cid;
				bool valid = cp_unpack_int(ctx->unpack, ctx->item, cid);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (!valid) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Expected Int");
					return true;
				}
				broker_lock(c->broker);
				valid = cid_valid(c->broker, cid);
				if (valid)
					rpcclient_disconnect(
						rpchandler_client(c->broker->clients[cid].handler));
				broker_unlock(c->broker);
				if (valid)
					rpchandler_msg_send_response_void(ctx);
				else
					rpchandler_msg_send_error(
						ctx, RPCERR_METHOD_CALL_EXCEPTION, "No such client");
				return true;
			}
		}
	return false;
}

static int subttlcmp(const void *a, const void *b) {
	const struct ttlsub *da = a;
	const struct ttlsub *db = b;
	return da->ttl - db->ttl;
}

static inline bool rpc_msg_request_broker_current_client(
	struct clientctx *c, struct rpchandler_msg *ctx) {
	const struct gperf_api_current_client_method_match *match =
		gperf_api_current_client_method(ctx->meta.method, strlen(ctx->meta.method));
	if (match != NULL)
		switch (match->method) {
			case M_INFO: {
				bool has_param = rpcmsg_has_value(ctx->item);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (has_param) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Must be 'null'");
					return true;
				}
				broker_lock(c->broker);
				send_client_info(c->broker, c - c->broker->clients, ctx);
				broker_unlock(c->broker);
				return true;
			}
			case M_SUBSCRIBE: {
				enum cpitem_type tp = cp_unpack_type(ctx->unpack, ctx->item);
				char *ri = NULL;
				int ttl = -1;
				if (tp == CPITEM_STRING) {
					ri = cp_unpack_strdupo(
						ctx->unpack, ctx->item, rpchandler_obstack(ctx));
				} else if (tp == CPITEM_LIST) {
					ri = cp_unpack_strdupo(
						ctx->unpack, ctx->item, rpchandler_obstack(ctx));
					if (ri)
						cp_unpack_int(ctx->unpack, ctx->item, ttl);
				}
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (ri == NULL) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Expected String or List");
					return true;
				}
				broker_lock(c->broker);
				bool res = subscribe(c->broker, ri, c - c->broker->clients);
				if (ttl > 0) {
					struct ttlsub *ttlsub = NULL;
					if (res) {
						ttlsub = ARR_ADD(c->ttlsubs);
						ttlsub->ri = subscription_ri(c->broker, ri);
					} else {
						for (size_t i = 0; i < c->ttlsubs_cnt; i++)
							if (!strcmp(ri, c->ttlsubs[i].ri)) {
								ttlsub = &c->ttlsubs[i];
								break;
							}
						assert(ttlsub);
					}
					ttlsub->ttl = get_time() + ttl;
					ARR_QSORT(c->ttlsubs, subttlcmp);
				}
				broker_unlock(c->broker);
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_bool(pack, res);
				rpchandler_msg_send_response(ctx, pack);
				return true;
			}
			case M_UNSUBSCRIBE: {
				char *ri = cp_unpack_strdupo(
					ctx->unpack, ctx->item, rpchandler_obstack(ctx));
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (ri == NULL) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Expected String");
					return true;
				}
				broker_lock(c->broker);
				for (size_t i = 0; i < c->ttlsubs_cnt; i++)
					if (!strcmp(ri, c->ttlsubs[i].ri)) {
						ARR_DEL(c->ttlsubs, c->ttlsubs + i);
						break;
					}
				bool res = unsubscribe(c->broker, ri, c - c->broker->clients);
				broker_unlock(c->broker);
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_bool(pack, res);
				cp_pack_container_end(pack);
				rpchandler_msg_send_response(ctx, pack);
				return true;
			}
			case M_SUBSCRIPTIONS: {
				bool has_param = rpcmsg_has_value(ctx->item);
				if (!rpchandler_msg_valid(ctx))
					return true;
				if (has_param) {
					rpchandler_msg_send_error(
						ctx, RPCERR_INVALID_PARAM, "Must be 'null'");
					return true;
				}
				broker_lock(c->broker);
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				pack_subscriptions(pack, c);
				broker_unlock(c->broker);
				rpchandler_msg_send_response(ctx, pack);
				return true;
			}
		}
	return false;
}

bool rpcbroker_api_msg(struct clientctx *c, struct rpchandler_msg *ctx) {
	if (!strcmp(ctx->meta.path, ".broker")) {
		return rpc_msg_request_broker(c, ctx);
	} else if (!strcmp(ctx->meta.path, ".broker/currentClient")) {
		return rpc_msg_request_broker_current_client(c, ctx);
	}
	return false;
}
