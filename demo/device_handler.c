#include "device_handler.h"
#include <stdlib.h>


static void rpc_ls(void *cookie, const char *path, struct rpchandler_ls_ctx *ctx) {
	if (path == NULL || *path == '\0')
		rpchandler_ls_result(ctx, "track");
	else if (!strcmp(path, "track")) {
		char str[2];
		str[1] = '\0';
		for (int i = 1; i <= 9; i++) {
			str[0] = '0' + i;
			rpchandler_ls_result(ctx, str);
		}
	}
}


static int trackid(const char *path) {
	const char prefix[] = "track/";
	const size_t prefix_len = sizeof(prefix) / sizeof(*prefix) - 1;
	if (path && !strncmp(path, prefix, prefix_len) && path[prefix_len] > '0' &&
		path[prefix_len] <= '9' && path[prefix_len + 1] == '\0') {
		return path[prefix_len] - '0' - 1;
	}
	return -1;
}


static void rpc_dir(void *cookie, const char *path, struct rpchandler_dir_ctx *ctx) {
	if (trackid(path) != -1) {
		rpchandler_dir_result(ctx, "get", RPCNODE_DIR_RET_PARAM,
			RPCNODE_DIR_F_GETTER, RPCMSG_ACC_READ, NULL);
		rpchandler_dir_result(ctx, "set", RPCNODE_DIR_VOID_PARAM,
			RPCNODE_DIR_F_SETTER, RPCMSG_ACC_WRITE, NULL);
	}
}

static enum rpchandler_func_res rpc_msg(
	void *cookie, struct rpcreceive *receive, const struct rpcmsg_meta *meta) {
	struct device_state *state = cookie;
	int tid = trackid(meta->path);
	if (tid != -1) {
		if (!strcmp(meta->method, "get")) {
			if (!rpcreceive_validmsg(receive))
				return RPCHFR_RECV_ERR;
			cp_pack_t pack = rpcreceive_response_new(receive);
			rpcmsg_pack_response(pack, meta);
			cp_pack_list_begin(pack);
			for (size_t i = 0; i < state->tracks[tid].cnt; i++)
				cp_pack_int(pack, state->tracks[tid].values[i]);
			cp_pack_container_end(pack);
			cp_pack_container_end(pack);
			rpcreceive_response_send(receive);
			return RPCHFR_HANDLED;
		} else if (!strcmp(meta->method, "set")) {
			size_t siz = 2;
			size_t cnt = 0;
			int *res = malloc(sizeof(int) * siz);
			bool invalid_param = false;
			cp_unpack(receive->unpack, &receive->item);
			if (receive->item.type == CPITEM_LIST) {
				do {
					cp_unpack(receive->unpack, &receive->item);
					if (receive->item.type == CPITEM_INT) {
						if (cnt >= siz)
							res = realloc(res, sizeof(int) * (siz *= 2));
						res[cnt++] = receive->item.as.Int;
						continue;
					} else if (receive->item.type != CPITEM_CONTAINER_END) {
						free(res);
						invalid_param = true;
					}
				} while (receive->item.type != CPITEM_CONTAINER_END);
			} else
				invalid_param = true;
			if (!rpcreceive_validmsg(receive))
				return RPCHFR_RECV_ERR;

			cp_pack_t pack = rpcreceive_response_new(receive);
			if (invalid_param)
				rpcmsg_pack_error(pack, meta, RPCMSG_E_INVALID_PARAMS,
					"Only list of integers is allowed");
			else {
				free(state->tracks[tid].values);
				state->tracks[tid].values = res;
				state->tracks[tid].cnt = cnt;
				rpcmsg_pack_response_void(pack, meta);
			}
			rpcreceive_response_send(receive);
			return RPCHFR_HANDLED;
		}
	}
	return RPCHFR_UNHANDLED;
}


const struct rpchandler_funcs device_handler_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};


struct device_state *device_state_new(void) {
	struct device_state *res = malloc(sizeof *res);
	for (int i = 0; i < 9; i++) {
		res->tracks[i].cnt = i + 1;
		res->tracks[i].values = malloc((i + 1) * sizeof(int));
		for (size_t y = 0; y <= i; y++)
			res->tracks[i].values[y] = y + 1;
	}
	return res;
}

void device_state_free(struct device_state *state) {
	for (int i = 0; i < 9; i++)
		free(state->tracks[i].values);
	free(state);
}
