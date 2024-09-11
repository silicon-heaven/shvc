#include <stdlib.h>
#include <unistd.h>
#include <shv/rpchandler_impl.h>
#include <shv/rpchandler_records.h>
#include <shv/rpchistory.h>

struct rpchandler_records {
	const char *name;
	void *log;
	struct rpchandler_records_ops *ops;
};

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_records *handler = cookie;
	if (!ctx->path || (ctx->path[0] == '\0'))
		rpchandler_ls_result(ctx, ".records");
	if (ctx->path && !strcmp(ctx->path, ".records"))
		rpchandler_ls_result(ctx, handler->name);
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	struct rpchandler_records *handler = cookie;
	char *path = malloc(strlen(".records/") + strlen(handler->name) + 1);
	strcpy(path, ".records/");
	strcat(path, handler->name);
	if (ctx->path && !strcmp(ctx->path, path)) {
		rpchandler_dir_result(ctx, &rpchistory_fetch);
		rpchandler_dir_result(ctx, &rpchistory_span);
	}

	free(path);
}

static void rpcspan_result(
	struct rpchandler_msg *ctx, uint64_t min, uint64_t max, uint64_t span) {
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_list_begin(pack);
	cp_pack_int(pack, min);
	cp_pack_int(pack, max);
	cp_pack_int(pack, span);
	cp_pack_container_end(pack);
	rpchandler_msg_send_response(ctx, pack);
}

static void rpcfetch_result(struct rpchandler_records *handler,
	struct rpchandler_msg *ctx, int start, int num) {
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_list_begin(pack);
	for (int i = start; i < start + num; i++) {
		/* There might be situation where we will not be able to read
		 * the record from the log and pack it, for example if the record was
		 * already erased from the logging facility prior to read operation.
		 * This is not an error as the standard does not guarantee the
		 * previously obtained record indexes are still valid. Regardless of
		 * what happens in the logging framework, we do not want to report error
		 * here; just pack the record if possible. If no record's pack is
		 * successful, just send back an empty list.
		 */
		handler->ops->pack_record(handler->log, i, pack, rpchandler_obstack(ctx));
	}

	cp_pack_container_end(pack);
	rpchandler_msg_send_response(ctx, pack);
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_records *handler = cookie;
	const char *const prefix = ".records/";
	if (ctx->meta.type == RPCMSG_T_REQUEST &&
		!strncmp(ctx->meta.path, prefix, strlen(prefix)) &&
		!strcmp(ctx->meta.path + strlen(prefix), handler->name)) {
		if (!strcmp(ctx->meta.method, "fetch") &&
			ctx->meta.access >= RPCACCESS_SERVICE) {
			size_t start;
			size_t num;
			bool invalid_param = false;
			if (cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_LIST ||
				!cp_unpack_int(ctx->unpack, ctx->item, start) ||
				!cp_unpack_int(ctx->unpack, ctx->item, num))
				invalid_param = true;

			if (!rpchandler_msg_valid(ctx))
				return RPCHANDLER_MSG_DONE;

			if (invalid_param) {
				rpchandler_msg_send_error(
					ctx, RPCERR_INVALID_PARAM, "[Int, Int] expected.");
			} else
				rpcfetch_result(handler, ctx, start, num);

			return RPCHANDLER_MSG_DONE;
		} else if (!strcmp(ctx->meta.method, "span") &&
			ctx->meta.access >= RPCACCESS_SERVICE) {
			if (rpchandler_msg_valid_nullparam(ctx)) {
				uint64_t min, max, span;
				if (handler->ops->get_index_range(handler->log, &min, &max, &span)) {
					rpcspan_result(ctx, min, max, span);
				} else
					rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM,
						"Could not retrieve information from the log.");
			}
			return RPCHANDLER_MSG_DONE;
		}
	}
	return RPCHANDLER_MSG_SKIP;
}

static const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};

rpchandler_records_t rpchandler_records_new(
	const char *name, void *log, struct rpchandler_records_ops *ops) {
	rpchandler_records_t res = malloc(sizeof *res);
	if (!res)
		return NULL;

	res->name = name;
	res->log = log;
	res->ops = ops;

	return res;
}

void rpchandler_records_destroy(rpchandler_records_t records) {
	free(records);
}

struct rpchandler_stage rpchandler_records_stage(rpchandler_records_t records) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = records};
}
