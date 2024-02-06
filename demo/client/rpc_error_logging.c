#include "rpc_error_logging.h"

void error_handler(const rpchandler_t err_handler, rpcclient_t err_client,
	enum rpchandler_error err_error) {
	fprintf(stderr, "An error occurred in the RPC handler.\n");
}

rpclogger_t initiate_logger_if_verbose(const struct conf conf, rpcclient_t client) {
	rpclogger_t logger = NULL;

	if (conf.verbose > 0) {
		logger = rpclogger_new(stderr, conf.verbose);
		client->logger = logger;
	}

	return logger;
}

void print_attached_error_message(rpc_request_ctx *ctx) {
	char *errmsg;
	rpcmsg_unpack_error(ctx->receive->unpack, &ctx->receive->item, NULL, &errmsg);
	fprintf(stderr, "Error: %s\n", errmsg);
	free(errmsg);
}

void print_unexpected_data_type_error(rpc_request_ctx *ctx,
	const char *method_name, enum cpitem_type expected_type) {
	fprintf(stderr,
		"Error: Unexpected data type for '%s'.\nExpected: %s, received: %s.\n",
		method_name, cpitem_type_str(expected_type),
		cpitem_type_str(ctx->receive->item.type));
}
