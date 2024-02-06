#include "rpc_request.h"

bool rpc_request_start(rpc_request_ctx *ctx, const char *path, const char *method) {
	if (ctx == NULL)
		return false;

	ctx->packer = rpchandler_msg_new(ctx->handler);
	ctx->request_id = rpchandler_next_request_id(ctx->handler);
	bool pack_success =
		rpcmsg_pack_request(ctx->packer, path, method, ctx->request_id);

	return pack_success;
}

bool rpc_request_send_and_receive(rpc_request_ctx *ctx) {
	if (ctx == NULL)
		return false;

	cp_pack_container_end(ctx->packer);
	ctx->expected_response =
		rpcresponse_expect(ctx->responses_handler, ctx->request_id);
	if (!rpchandler_msg_send(ctx->handler)) {
		fprintf(stderr,
			"Error: Message sending failed. Enable verbose mode for details.\n");
		return false;
	}

	bool response_received = rpcresponse_waitfor(
		ctx->expected_response, &ctx->receive, &ctx->meta, ctx->timeout);
	if (!response_received) {
		fprintf(stderr, "Error: Response timeout.\n");
		return false;
	}

	return true;
}
