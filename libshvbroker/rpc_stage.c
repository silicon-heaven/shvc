#include <shv/cp_tools.h>
#include <shv/rpcri.h>

#include "api.h"
#include "broker.h"
#include "stages.h"
#include "util.h"


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

static inline bool rpc_msg_request(struct clientctx *c, struct rpchandler_msg *ctx) {
	rpcaccess_t newaccess = c->role
		? c->role->access(c->role->access_cookie, ctx->meta.path, ctx->meta.method)
		: RPCACCESS_NONE;
	ctx->meta.access = ctx->meta.access < newaccess ? ctx->meta.access : newaccess;
	if (rpcbroker_api_msg(c, ctx))
		return true;

	/* Propagate to some mount point if not handled locally */
	broker_lock(c->broker);
	const char *rpath;
	struct clientctx *client = mounted_client(c->broker, ctx->meta.path, &rpath);
	if (!client) {
		broker_unlock(c->broker);
		return false;
	}

	struct obstack *obs = rpchandler_obstack(ctx);
	ctx->meta.path = (char *)rpath;
	long long *ncids = obstack_alloc(obs, (ctx->meta.cids_cnt + 1) * sizeof *ncids);
	memcpy(ncids, ctx->meta.cids, ctx->meta.cids_cnt * sizeof *ncids);
	ncids[ctx->meta.cids_cnt++] = c - c->broker->clients;
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
	return true;
}

static inline bool rpc_msg_response(
	struct clientctx *c, struct rpchandler_msg *ctx) {
	broker_lock(c->broker);
	if (ctx->meta.cids_cnt == 0 ||
		!cid_valid(c->broker, ctx->meta.cids[ctx->meta.cids_cnt - 1])) {
		broker_unlock(c->broker);
		return false;
	}

	struct clientctx *dest =
		&c->broker->clients[ctx->meta.cids[ctx->meta.cids_cnt - 1]];
	ctx->meta.cids_cnt--;
	propagate_msg(ctx, dest);
	return true;
}

struct multipack {
	cp_pack_func_t func;
	cp_pack_t *packs;
	size_t cnt;
};
static bool multipack_func(void *ptr, const struct cpitem *item) {
	struct multipack *p = ptr;
	bool res = false;
	for (size_t i = 0; i < p->cnt; i++)
		if (p->packs[i])
			res |= cp_pack(p->packs[i], item);
	return res;
}

static inline bool rpc_msg_signal(struct clientctx *c, struct rpchandler_msg *ctx) {
	broker_lock(c->broker);
	if (!c->role || !c->role->mount_point)
		return false;
	if (ctx->meta.path && *ctx->meta.path != '\0') {
		struct obstack *obs = rpchandler_obstack(ctx);
		obstack_printf(obs, "%s/%s", c->role->mount_point, ctx->meta.path);
		ctx->meta.path = obstack_finish(obs);
	} else
		ctx->meta.path = (char *)c->role->mount_point;

	nbool_t dest = NULL;
	for (size_t i = 0; i < c->broker->subscriptions_cnt; i++)
		if (rpcri_match(c->broker->subscriptions[i].ri, ctx->meta.path,
				ctx->meta.source, ctx->meta.signal))
			nbool_or(&dest, c->broker->subscriptions[i].clients);
	if (dest == NULL) /* Not handling. Nobody cares about it */
		return false;
	size_t siz = 0;
	for_nbool(dest, cid) siz++;
	cp_pack_t packs[siz];
	size_t packi = 0;
	for_nbool(dest, cid) {
		if (cid_active(c->broker, cid) &&
			c->role->access(c->role->access_cookie, ctx->meta.path,
				ctx->meta.method) >= ctx->meta.access)
			packs[packi++] = rpchandler_msg_new(c->broker->clients[cid].handler);
		else
			siz--;
	}
	struct multipack mp = {multipack_func, packs, siz};
	cp_pack_t pack = &mp.func;
	if (rpcmsg_has_value(ctx->item)) {
		rpcmsg_pack_meta(pack, &ctx->meta);
		cp_repack(ctx->unpack, ctx->item, pack);
		cp_pack_container_end(pack);
	} else
		rpcmsg_pack_meta_void(pack, &ctx->meta);
	bool send = rpchandler_msg_valid(ctx);
	for_nbool(dest, cid) {
		if (cid_active(c->broker, cid)) {
			if (send)
				rpchandler_msg_send(c->broker->clients[cid].handler);
			else
				rpchandler_msg_drop(c->broker->clients[cid].handler);
		}
	}
	return true;
}

static bool rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
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
	return false;
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
	time_t now = get_time();
	broker_lock(c->broker);

	size_t i;
	for (i = 0; i < c->ttlsubs_cnt && c->ttlsubs[i].ttl <= now; i++)
		unsubscribe(c->broker, c->ttlsubs[i].ri, c - c->broker->clients);
	ARR_DROP(c->ttlsubs, i);
	if (c->ttlsubs_cnt > 0) {
		int ttlres = (c->ttlsubs[0].ttl - now) * 1000;
		if (ttlres < res)
			res = ttlres;
	}

	// TODO subbobroker subscribe
	broker_unlock(c->broker);
	return res;
}

static void rpc_reset(void *cookie) {
	struct clientctx *c = cookie;
	broker_lock(c->broker);
	unsubscribe_all(c->broker, c - c->broker->clients);
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
