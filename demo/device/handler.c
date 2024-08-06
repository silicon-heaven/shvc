#include "handler.h"
#include <stdlib.h>
#include <shv/rpchandler_impl.h>


static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	if (ctx->path && ctx->path[0] == '\0')
		rpchandler_ls_result(ctx, "track");
	else if (ctx->path && !strcmp(ctx->path, "track")) {
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


static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	if (trackid(ctx->path) != -1) {
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "get",
				.result = "[Int]",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_READ,
				.signals = (const struct rpcdirsig[]){{.name = "chng"}},
				.signals_cnt = 1,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "set",
				.param = "[Int]",
				.flags = RPCDIR_F_SETTER,
				.access = RPCACCESS_WRITE,
			});
	}
}

static bool rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct device_state *state = cookie;
	int tid = trackid(ctx->meta.path);
	if (tid != -1) {
		if (!strcmp(ctx->meta.method, "get")) {
			if (!rpchandler_msg_valid(ctx))
				return true;
			if (!rpchandler_msg_access_level(ctx, RPCACCESS_READ))
				return true;
			cp_pack_t pack = rpchandler_msg_new_response(ctx);
			cp_pack_list_begin(pack);
			for (size_t i = 0; i < state->tracks[tid].cnt; i++)
				cp_pack_int(pack, state->tracks[tid].values[i]);
			cp_pack_container_end(pack);
			cp_pack_container_end(pack);
			rpchandler_msg_send(ctx);
			return true;
		} else if (!strcmp(ctx->meta.method, "set")) {
			size_t siz = 2;
			size_t cnt = 0;
			int *res = malloc(sizeof(int) * siz);
			bool invalid_param = false;
			cp_unpack(ctx->unpack, ctx->item);
			if (ctx->item->type == CPITEM_LIST) {
				for_cp_unpack_list(ctx->unpack, ctx->item) {
					if (ctx->item->type != CPITEM_INT) {
						invalid_param = true;
						break;
					}
					if (cnt >= siz) {
						int *tmp_res = realloc(res, sizeof(int) * (siz *= 2));
						assert(tmp_res);
						res = tmp_res;
					}
					res[cnt++] = ctx->item->as.Int;
				}
			} else
				invalid_param = true;
			if (!rpchandler_msg_valid(ctx) ||
				!rpchandler_msg_access_level(ctx, RPCACCESS_WRITE)) {
				free(res);
				return true;
			}

			bool value_changed = false;
			if (invalid_param) {
				free(res);
				rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM,
					"Only list of integers is allowed");
			} else {
				value_changed = state->tracks[tid].cnt != cnt ||
					memcmp(state->tracks[tid].values, res, cnt);
				free(state->tracks[tid].values);
				state->tracks[tid].values = res;
				state->tracks[tid].cnt = cnt;
				rpchandler_msg_send_response_void(ctx);
			}

			if (value_changed) {
				cp_pack_t pack = rpchandler_msg_new(ctx);
				rpcmsg_pack_signal(
					pack, ctx->meta.path, "get", "chng", NULL, RPCACCESS_READ);
				cp_pack_list_begin(pack);
				for (size_t i = 0; i < cnt; i++)
					cp_pack_int(pack, res[i]);
				cp_pack_container_end(pack);
				cp_pack_container_end(pack);
				rpchandler_msg_send(ctx);
			}
			return true;
		}
	}
	return false;
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
