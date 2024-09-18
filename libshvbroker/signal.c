#include <shv/rpcbroker.h>
#include "multipack.h"

struct sigctx {
	struct rpcbroker_sigctx pub;
	rpcbroker_t broker;
	nbool_t destinations;
	struct multipack multipack;
};

static inline struct sigctx *init(rpcbroker_t broker, const char *path,
	const char *source, const char *signal, rpcaccess_t access) {
	nbool_t dest = signal_destinations(broker, path, source, signal, access);
	if (dest == NULL)
		return NULL;
	struct sigctx *res = malloc(sizeof *res);
	res->broker = broker;
	res->destinations = dest;
	res->pub.pack = multipack_init(broker, &res->multipack, dest);
	return res;
}

struct rpcbroker_sigctx *rpcbroker_new_signal(rpcbroker_t broker,
	const char *path, const char *source, const char *signal, const char *uid,
	rpcaccess_t access, bool repeat) {
	broker_lock(broker);
	struct sigctx *ctx = init(broker, path, source, signal, access);
	struct rpcbroker_sigctx *res = NULL;
	if (ctx) {
		rpcmsg_pack_signal(
			ctx->pub.pack, path, source, signal, uid, access, repeat);
		res = &ctx->pub;
	}
	broker_unlock(broker);
	return res;
}

static inline bool done(struct sigctx *ctx, bool val) {
	rpcbroker_t broker = ctx->broker;
	broker_lock(broker);
	multipack_done(broker, &ctx->multipack, ctx->destinations, val);
	free(ctx->destinations);
	free(ctx);
	broker_unlock(broker);
	return true;
}

bool rpcbroker_send_signal(struct rpcbroker_sigctx *ctx) {
	return done((struct sigctx *)ctx, true);
}

bool rpcbroker_drop_signal(struct rpcbroker_sigctx *ctx) {
	return done((struct sigctx *)ctx, false);
}

bool rpcbroker_send_signal_void(rpcbroker_t broker, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access,
	bool repeat) {
	broker_lock(broker);
	struct sigctx *ctx = init(broker, path, source, signal, access);
	bool res = true;
	if (ctx) {
		rpcmsg_pack_signal_void(
			ctx->pub.pack, path, source, signal, uid, access, repeat);
		res = done(ctx, true);
	}
	broker_unlock(broker);
	return res;
}
