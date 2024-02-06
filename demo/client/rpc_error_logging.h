#ifndef _DEMO_RPC_ERROR_LOGGING_H
#define _DEMO_RPC_ERROR_LOGGING_H
#include "opts.h"
#include "rpc_request.h"
#include <malloc.h>
#include <shv/rpchandler_app.h>

void error_handler(const rpchandler_t err_handler, rpcclient_t err_client,
	enum rpchandler_error err_error);
rpclogger_t initiate_logger_if_verbose(const struct conf conf, rpcclient_t client);
void print_attached_error_message(rpc_request_ctx *ctx);
void print_unexpected_data_type_error(rpc_request_ctx *ctx,
	const char *method_name, enum cpitem_type expected_type);

#endif
