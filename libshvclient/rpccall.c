#include <shv/rpccall.h>
#include <semaphore.h>

struct callctx {
	rpccall_func_t func;
	void *ctx;
	int res;
};

static bool response_callback(
	struct rpcreceive *receive, const struct rpcmsg_meta *meta, void *ctx) {
	struct callctx *cctx = ctx;
	cctx->res = cctx->func(meta->type == RPCMSG_T_RESPONSE
			? (rpcreceive_has_param(receive) ? CALL_S_RESULT : CALL_S_VOID_RESULT)
			: CALL_S_ERROR,
		NULL, meta->request_id, receive->unpack, &receive->item, cctx->ctx);
	return rpcreceive_validmsg(receive);
}

int _rpccall(rpchandler_t handler, rpchandler_responses_t responses,
	rpccall_func_t func, void *ctx, int attempts, int timeout) {
	struct callctx cctx;
	cctx.func = func;
	cctx.ctx = ctx;

	int request_id = rpchandler_next_request_id(handler);
	rpcresponse_t response =
		rpcresponse_expect(responses, request_id, response_callback, &cctx);
	for (int attempt = 0; attempt < attempts; attempt++) {
		cp_pack_t pack = rpchandler_msg_new(handler);
		int res = func(CALL_S_PACK, pack, request_id, NULL, NULL, ctx);
		if (res) {
			rpchandler_msg_drop(handler);
			rpcresponse_discard(response);
			return res;
		}
		if (!rpchandler_msg_send(handler)) {
			rpcresponse_discard(response);
			return func(CALL_S_COMERR, NULL, request_id, NULL, NULL, ctx);
		}
		if (rpcresponse_waitfor(response, timeout)) {
			return cctx.res;
		}
	}
	rpcresponse_discard(response);
	return func(CALL_S_TIMERR, NULL, request_id, NULL, NULL, ctx);
}
