#include <stdlib.h>
#include <semaphore.h>
#include <shv/rpccall.h>

struct _ctx {
	struct rpccall_ctx pub;
	rpccall_func_t func;
};

static bool response_callback(struct rpchandler_msg *ctx, void *cookie) {
	struct _ctx *c = cookie;
	if (ctx->meta.type == RPCMSG_T_ERROR) {
		rpcerror_unpack(ctx->unpack, ctx->item, &c->pub.errnum, &c->pub.errmsg);
		if (rpchandler_msg_valid(ctx))
			return true;
		free(c->pub.errmsg);
		c->pub.errnum = RPCERR_NO_ERROR;
		c->pub.errmsg = NULL;
		return false;
	}

	if (rpcmsg_has_value(ctx->item)) {
		c->pub.unpack = ctx->unpack;
		c->pub.item = ctx->item;
		c->func(CALL_S_RESULT, &c->pub);
	}
	if (!rpchandler_msg_valid(ctx))
		return false;
	c->pub.errnum = RPCERR_NO_ERROR;
	c->pub.errmsg = NULL;
	return true;
}

int _rpccall(rpchandler_t handler, rpchandler_responses_t responses,
	rpccall_func_t func, void *cookie, int attempts, int timeout) {
	struct _ctx ctx = {
		.pub.cookie = cookie,
		.pub.request_id = rpcmsg_request_id(),
		.pub.errnum = RPCERR_NO_ERROR,
		.pub.errmsg = NULL,
		.func = func,
	};
	rpcresponse_t response = rpcresponse_expect(
		responses, ctx.pub.request_id, response_callback, &ctx);
	int result = 0;

	for (int attempt = 0; attempt < attempts; attempt++) {
		cp_pack_t pack = rpchandler_msg_new(handler);
		ctx.pub.pack = pack;
		result = func(CALL_S_REQUEST, &ctx.pub);
		if (result) {
			rpchandler_msg_drop(handler);
			rpcresponse_discard(response);
			goto term;
		}
		if (!rpchandler_msg_send(handler)) {
			rpcresponse_discard(response);
			result = func(CALL_S_COMERR, &ctx.pub);
			goto term;
		}
		if (rpcresponse_waitfor(response, timeout)) {
			response = NULL;
			result = func(CALL_S_DONE, &ctx.pub);
			goto term;
		}
	}
	result = func(CALL_S_TIMERR, &ctx.pub);

term:
	free(ctx.pub.errmsg);
	rpcresponse_discard(response);
	return result;
}
