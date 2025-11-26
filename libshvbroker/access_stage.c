#include "api_login.h"
#include "broker.h"
#include "stages.h"


static enum rpchandler_msg_res access_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct clientctx *c = cookie;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	c->last_activity = now.tv_sec;
	if (c->role == NULL) {
		if (ctx->meta.type == RPCMSG_T_REQUEST)
			rpcbroker_api_login_msg(c, ctx);
		return RPCHANDLER_MSG_DONE;
	}

	if (ctx->meta.type == RPCMSG_T_REQUEST ||
		ctx->meta.type == RPCMSG_T_REQUEST_ABORT) {
		rpcaccess_t newaccess = RPCACCESS_NONE;
		if (c->role)
			newaccess = c->role->access(
				c->role->access_cookie, ctx->meta.path, ctx->meta.method);
		ctx->meta.access = ctx->meta.access < newaccess ? ctx->meta.access
														: newaccess;
	}
	return RPCHANDLER_MSG_SKIP;
}

static int access_idle(void *cookie, struct rpchandler_idle *ctx) {
	struct clientctx *c = cookie;
	broker_lock(c->broker);
	int res;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (c->activity_timeout != -1) {
		int timeout = c->last_activity + c->activity_timeout - now.tv_sec;
		res = timeout >= 0 ? timeout : RPCHANDLER_IDLE_STOP;
	} else
		res = RPCHANDLER_IDLE_SKIP;
	broker_unlock(c->broker);
	return res;
}

static void access_reset(void *cookie) {
	struct clientctx *c = cookie;
	broker_lock(c->broker);
	if (c->login) {
		role_unassign(c);
		c->nonce[0] = '\0';
		free(c->username);
		c->username = NULL;
	}
	broker_unlock(c->broker);
}

const struct rpchandler_funcs access_funcs = {
	.msg = access_msg,
	.idle = access_idle,
	.reset = access_reset,
};
