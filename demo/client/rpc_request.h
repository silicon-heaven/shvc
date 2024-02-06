#ifndef _DEMO_RPC_REQUEST_H
#define _DEMO_RPC_REQUEST_H
#include <stdbool.h>
#include "opts.h"
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_responses.h>

typedef struct rpc_request_ctx {
	rpchandler_t handler;
	rpchandler_responses_t responses_handler;
	rpcresponse_t expected_response;
	cp_pack_t packer;
	int request_id;
	int timeout;
	struct rpcreceive *receive;
	const struct rpcmsg_meta *meta;
} rpc_request_ctx;

bool rpc_request_start(rpc_request_ctx *ctx, const char *path, const char *method);
bool rpc_request_send_and_receive(rpc_request_ctx *ctx);

#endif
