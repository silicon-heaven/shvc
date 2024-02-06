#include "rpc_device_operations.h"

bool is_device_mounted(rpc_request_ctx *ctx) {
	if (ctx == NULL)
		return false;

	rpc_request_start(ctx, "test", "ls");
	cp_pack_str(ctx->packer, "shvc-demo");
	bool receive_success = rpc_request_send_and_receive(ctx);
	if (!receive_success) {
		fprintf(stderr, "Error: 'test:ls' request failed or timed out.\n");
		return false;
	}

	bool device_mounted = false;

	if (!rpcreceive_has_param(ctx->receive)) {
		fprintf(stderr, "Error: No parameter to unpack for 'test:ls' request.\n");
		return false;
	}

	if (ctx->meta->type == RPCMSG_T_ERROR) {
		print_attached_error_message(ctx);
		return false;
	} else if (ctx->meta->type == RPCMSG_T_RESPONSE) {
		cp_unpack(ctx->receive->unpack, &ctx->receive->item);
		if (ctx->receive->item.type == CPITEM_BOOL) {
			device_mounted = ctx->receive->item.as.Bool;
		} else {
			print_unexpected_data_type_error(ctx, "test:ls", CPITEM_BOOL);
			return false;
		}
	}

	if (!rpcresponse_validmsg(ctx->expected_response)) {
		fprintf(stderr,
			"Error: Invalid response received for 'test:ls' request.\n");
		return false;
	}

	return device_mounted;
}

bool get_app_name(rpc_request_ctx *ctx, char **out_app_name) {
	rpc_request_start(ctx, "test/shvc-demo/.app", "name");
	cp_pack_null(ctx->packer);
	bool app_name_receive_success = rpc_request_send_and_receive(ctx);
	if (!app_name_receive_success) {
		fprintf(stderr, "Error: '.app:name' request failed or timed out.\n");
		return false;
	}

	if (!rpcreceive_has_param(ctx->receive)) {
		fprintf(stderr, "Error: No parameter received for '.app:name' request.\n");
		return false;
	}

	if (ctx->meta->type == RPCMSG_T_ERROR) {
		print_attached_error_message(ctx);
		return false;
	} else if (ctx->meta->type == RPCMSG_T_RESPONSE) {
		cp_unpack(ctx->receive->unpack, &ctx->receive->item);
		if (ctx->receive->item.type == CPITEM_STRING) {
			*out_app_name =
				cp_unpack_strdup(ctx->receive->unpack, &ctx->receive->item);
			assert(out_app_name);
			rpcresponse_validmsg(ctx->expected_response);
		} else {
			print_unexpected_data_type_error(ctx, ".app:name", CPITEM_STRING);
			return false;
		}
	}

	return true;
}

bool track_getter_call(rpc_request_ctx *ctx) {
	if (ctx == NULL)
		return false;

	rpc_request_start(ctx, "test/shvc-demo/track/" TRACK_ID, "get");
	cp_pack_null(ctx->packer);
	bool track_value_receive_success = rpc_request_send_and_receive(ctx);
	if (!track_value_receive_success) {
		fprintf(stderr,
			"Error: 'track/" TRACK_ID
			":get' request request failed or timed out.\n");
		return false;
	}

	if (!rpcreceive_has_param(ctx->receive)) {
		fprintf(stderr,
			"Error: No parameter received for 'track/" TRACK_ID
			":get' request.\n");
		return false;
	}

	if (ctx->meta->type == RPCMSG_T_ERROR) {
		print_attached_error_message(ctx);
		return false;
	} else if (ctx->meta->type == RPCMSG_T_RESPONSE) {
		cp_unpack(ctx->receive->unpack, &ctx->receive->item);
		if (ctx->receive->item.type != CPITEM_LIST) {
			print_unexpected_data_type_error(
				ctx, "track/" TRACK_ID ":get", CPITEM_LIST);
			return false;
		}
	}

	return true;
}

bool track_getter_result(
	rpc_request_ctx *ctx, long long **out_buf_data, size_t *out_buf_length) {
	if (ctx == NULL || out_buf_data == NULL || *out_buf_data == NULL)
		return false;

	bool success = true;
	size_t capacity = 2;

	do {
		cp_unpack(ctx->receive->unpack, &ctx->receive->item);
		if (ctx->receive->item.type == CPITEM_CONTAINER_END)
			continue;

		if (ctx->receive->item.type != CPITEM_INT) {
			print_unexpected_data_type_error(
				ctx, "track/" TRACK_ID ":get", CPITEM_INT);
			success = false;
			continue;
		}

		if (*out_buf_length >= capacity) {
			long long *tmp_vec_data = *out_buf_data;
			*out_buf_data =
				realloc(*out_buf_data, sizeof(long long) * (capacity *= 2));

			if (*out_buf_data == NULL) {
				fprintf(stderr,
					"Error: Couldn't dynamically allocate new memory for larger vector.\n");
				*out_buf_data = tmp_vec_data;
				success = false;
				continue;
			}
		}

		(*out_buf_data)[(*out_buf_length)++] = ctx->receive->item.as.Int;
	} while (success && ctx->receive->item.type != CPITEM_CONTAINER_END);

	if (success) {
		rpcresponse_validmsg(ctx->expected_response);
	}

	return success;
}

bool try_set_track(
	rpc_request_ctx *ctx, const long long *buf_data, const size_t buf_length) {
	if (ctx == NULL || buf_data == NULL)
		return false;

	rpc_request_start(ctx, "test/shvc-demo/track/" TRACK_ID, "set");
	cp_pack_list_begin(ctx->packer);
	for (size_t i = 0; i < buf_length; i++) {
		cp_pack_int(ctx->packer, buf_data[i]);
	}
	cp_pack_container_end(ctx->packer);
	bool track_value_receive_success = rpc_request_send_and_receive(ctx);
	if (!track_value_receive_success) {
		fprintf(stderr,
			"Error: 'track/" TRACK_ID ":set' request failed or timed out.\n");
		return false;
	}

	if (ctx->receive->item.type == CPITEM_CONTAINER_END) {
		rpcresponse_validmsg(ctx->expected_response);
		return true;
	} else {
		return false;
	}
}
