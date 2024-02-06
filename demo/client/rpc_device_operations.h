#ifndef _DEMO_RPC_DEVICE_OPERATIONS_H
#define _DEMO_RPC_DEVICE_OPERATIONS_H
#include <malloc.h>
#include <stdio.h>
#include "rpc_error_logging.h"
#include "rpc_request.h"
#include "utils.h"
#include "opts.h"

bool is_device_mounted(rpc_request_ctx *ctx);
bool get_app_name(rpc_request_ctx *ctx, char **out_app_name);
bool track_getter_call(rpc_request_ctx *ctx);
bool track_getter_result(
	rpc_request_ctx *ctx, long long **out_buf_data, size_t *out_buf_length);
bool try_set_track(
	rpc_request_ctx *ctx, const long long *buf_data, const size_t buf_length);

#endif
