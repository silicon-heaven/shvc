#include <shv/rpccall.h>
#include <semaphore.h>

struct callctx {
	rpccall_func_t func;
	void *cookie;
	int res;
};

static bool response_callback(struct rpchandler_msg *ctx, void *cookie) {
	struct callctx *c = cookie;
	c->res = c->func(ctx->meta.type == RPCMSG_T_RESPONSE
			? (rpcmsg_has_param(ctx->item) ? CALL_S_RESULT : CALL_S_VOID_RESULT)
			: CALL_S_ERROR,
		NULL, ctx->meta.request_id, ctx->unpack, ctx->item, c->cookie);
	return rpchandler_msg_valid(ctx);
}

int _rpccall(rpchandler_t handler, rpchandler_responses_t responses,
	rpccall_func_t func, void *cookie, int attempts, int timeout) {
	struct callctx cctx;
	cctx.func = func;
	cctx.cookie = cookie;

	int request_id = rpcmsg_request_id();
	rpcresponse_t response =
		rpcresponse_expect(responses, request_id, response_callback, &cctx);
	for (int attempt = 0; attempt < attempts; attempt++) {
		cp_pack_t pack = rpchandler_msg_new(handler);
		int res = func(CALL_S_PACK, pack, request_id, NULL, NULL, cookie);
		if (res) {
			rpchandler_msg_drop(handler);
			rpcresponse_discard(response);
			return res;
		}
		if (!rpchandler_msg_send(handler)) {
			rpcresponse_discard(response);
			return func(CALL_S_COMERR, NULL, request_id, NULL, NULL, cookie);
		}
		if (rpcresponse_waitfor(response, timeout)) {
			return cctx.res;
		}
	}
	rpcresponse_discard(response);
	return func(CALL_S_TIMERR, NULL, request_id, NULL, NULL, cookie);
}
