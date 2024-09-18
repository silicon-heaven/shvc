#include <shv/cp_tools.h>
#include <shv/rpcri.h>

#include "api.h"
#include "broker.h"
#include "multipack.h"
#include "stages.h"


static void propagate_msg(struct rpchandler_msg *ctx, struct clientctx *client) {
	rpchandler_t handler = client->handler;
	cp_pack_t pack = rpchandler_msg_new(handler);
	broker_unlock(client->broker); /* We can unlock now, we have handler lock */
	if (rpcmsg_has_value(ctx->item)) {
		rpcmsg_pack_meta(pack, &ctx->meta);
		cp_repack(ctx->unpack, ctx->item, pack);
		cp_pack_container_end(pack);
	} else
		rpcmsg_pack_meta_void(pack, &ctx->meta);
	if (rpchandler_msg_valid(ctx))
		rpchandler_msg_send(handler);
	else
		rpchandler_msg_drop(handler);
}

static inline enum rpchandler_msg_res rpc_msg_request(
	struct clientctx *c, struct rpchandler_msg *ctx) {
	rpcaccess_t newaccess = c->role
		? c->role->access(c->role->access_cookie, ctx->meta.path, ctx->meta.method)
		: RPCACCESS_NONE;
	ctx->meta.access = ctx->meta.access < newaccess ? ctx->meta.access : newaccess;
	if (rpcbroker_api_msg(c, ctx))
		return RPCHANDLER_MSG_DONE;

	/* Propagate to some mount point if not handled locally */
	broker_lock(c->broker);
	const char *rpath;
	struct clientctx *client = mounted_client(c->broker, ctx->meta.path, &rpath);
	if (!client) {
		broker_unlock(c->broker);
		return RPCHANDLER_MSG_SKIP;
	}

	struct obstack *obs = rpchandler_obstack(ctx);
	ctx->meta.path = (char *)rpath;
	long long *ncids = obstack_alloc(obs, (ctx->meta.cids_cnt + 1) * sizeof *ncids);
	memcpy(ncids, ctx->meta.cids, ctx->meta.cids_cnt * sizeof *ncids);
	ncids[ctx->meta.cids_cnt++] = c->cid;
	ctx->meta.cids = ncids;
	if (ctx->meta.user_id) {
		if (*ctx->meta.user_id != '\0') {
			obstack_grow(obs, ctx->meta.user_id, strlen(ctx->meta.user_id));
			obstack_grow(obs, ";", 1);
		}
		if (c->username)
			obstack_grow(obs, c->username, strlen(c->username));
		if (c->username && c->broker->name)
			obstack_grow(obs, ":", 1);
		if (c->broker->name)
			obstack_grow(obs, c->broker->name, strlen(c->broker->name));
		obstack_1grow(obs, '\0');
		ctx->meta.user_id = obstack_finish(obs);
	}
	propagate_msg(ctx, client);
	return RPCHANDLER_MSG_DONE;
}

static inline enum rpchandler_msg_res rpc_msg_response(
	struct clientctx *c, struct rpchandler_msg *ctx) {
	broker_lock(c->broker);
	if (ctx->meta.cids_cnt == 0 ||
		!cid_valid(c->broker, ctx->meta.cids[ctx->meta.cids_cnt - 1])) {
		broker_unlock(c->broker);
		return RPCHANDLER_MSG_SKIP;
	}

	struct clientctx *dest =
		c->broker->clients[ctx->meta.cids[ctx->meta.cids_cnt - 1]];
	ctx->meta.cids_cnt--;
	propagate_msg(ctx, dest);
	return RPCHANDLER_MSG_DONE;
}

static inline enum rpchandler_msg_res rpc_msg_signal(
	struct clientctx *c, struct rpchandler_msg *ctx) {
	broker_lock(c->broker);
	if (!c->role || !c->role->mount_point)
		return RPCHANDLER_MSG_SKIP;
	if (ctx->meta.path && *ctx->meta.path != '\0') {
		struct obstack *obs = rpchandler_obstack(ctx);
		obstack_printf(obs, "%s/%s", c->role->mount_point, ctx->meta.path);
		obstack_1grow(obs, '\0');
		ctx->meta.path = obstack_finish(obs);
	} else
		ctx->meta.path = (char *)c->role->mount_point;

	nbool_t dest = signal_destinations(c->broker, ctx->meta.path,
		ctx->meta.source, ctx->meta.signal, ctx->meta.access);
	if (dest == NULL) /* Not handling. Nobody cares about it */
		return RPCHANDLER_MSG_SKIP;
	struct multipack multipack;
	cp_pack_t pack = multipack_init(c->broker, &multipack, dest);
	if (rpcmsg_has_value(ctx->item)) {
		rpcmsg_pack_meta(pack, &ctx->meta);
		cp_repack(ctx->unpack, ctx->item, pack);
		cp_pack_container_end(pack);
	} else
		rpcmsg_pack_meta_void(pack, &ctx->meta);
	multipack_done(c->broker, &multipack, dest, rpchandler_msg_valid(ctx));
	free(dest);
	return RPCHANDLER_MSG_SKIP;
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct clientctx *c = cookie;
	switch (ctx->meta.type) {
		case RPCMSG_T_REQUEST:
			return rpc_msg_request(c, ctx);
		case RPCMSG_T_RESPONSE:
		case RPCMSG_T_ERROR:
			return rpc_msg_response(c, ctx);
		case RPCMSG_T_SIGNAL:
			return rpc_msg_signal(c, ctx);
		case RPCMSG_T_INVALID:
			break;
	}
	return RPCHANDLER_MSG_SKIP;
}

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	rpcbroker_api_ls(cookie, ctx);
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	rpcbroker_api_dir(ctx);
}

int rpc_idle(void *cookie, struct rpchandler_idle *ctx) {
	struct clientctx *c = cookie;
	int res = INT_MAX;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	broker_lock(c->broker);

	size_t i;
	for (i = 0; i < c->ttlsubs_cnt && c->ttlsubs[i].ttl <= now.tv_sec; i++)
		unsubscribe(c->broker, c->ttlsubs[i].ri, c->cid);
	ARR_DROP(c->ttlsubs, i);
	if (c->ttlsubs_cnt > 0) {
		int ttlres = (c->ttlsubs[0].ttl - now.tv_sec) * 1000;
		if (ttlres < res)
			res = ttlres;
	}

	// TODO subbroker subscribe
	broker_unlock(c->broker);
	return res;
}

static void rpc_reset(void *cookie) {
	struct clientctx *c = cookie;
	broker_lock(c->broker);
	unsubscribe_all(c->broker, c->cid);
	ARR_RESET(c->ttlsubs);
	broker_unlock(c->broker);
}

const struct rpchandler_funcs rpc_funcs = {
	.msg = rpc_msg,
	.ls = rpc_ls,
	.dir = rpc_dir,
	.idle = rpc_idle,
	.reset = rpc_reset,
};
