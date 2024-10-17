#include <stdlib.h>
#include <unistd.h>
#include <shv/rpchandler_history.h>
#include <shv/rpchandler_impl.h>
#include <shv/rpchistory.h>

#define RECORDS_PREFIX ".records/"
#define FILES_PREFIX ".files/"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

struct rpchandler_history {
	bool has_records;
	bool has_files;
	bool has_getlog;
	const char **signals;
	struct rpchandler_history_facilities *facilities;
};

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_history *history = cookie;
	if (!ctx->path || (ctx->path[0] == '\0')) {
		if (history->has_records)
			rpchandler_ls_result(ctx, ".records");
		if (history->has_files)
			rpchandler_ls_result(ctx, ".files");
		if (history->has_getlog) {
			/* Handle virtual tree generated for getLog method. This takes care
			 * of the root nodes.
			 */
			for (const char **s = history->signals; s && *s; s++)
				rpchandler_ls_result_fmt(
					ctx, "%.*s", (int)(strchrnul(*s, '/') - *s), *s);
		}
	} else if (!strcmp(ctx->path, ".records")) {
		for (struct rpchandler_history_records **record =
				 history->facilities->records;
			 record && *record; record++)
			rpchandler_ls_result(ctx, (*record)->name);
	} else if (!strcmp(ctx->path, ".files")) {
		for (struct rpchandler_history_files **file = history->facilities->files;
			 file && *file; file++)
			rpchandler_ls_result(ctx, (*file)->name);
	} else if (history->has_getlog) {
		/* Handle getLog virtual tree for all subnodes. This requires parsing
		 * of signal paths to separate nodes and correctly identifying children
		 * nodes present on the given path.
		 */
		for (const char **s = history->signals; s && *s; s++) {
			/* Continue if ctx->path substring does not start at given signal
			 * path
			 */
			size_t len = strlen(ctx->path);
			if (strncmp(*s, ctx->path, len) || (*s)[len] != '/')
				continue;

			/* Get the starting position of path's children. */
			const char *start = *s + len;
			/* The children's end is the next '/' character. */
			const char *end = strchrnul(++start, '/');
			rpchandler_ls_result_fmt(ctx, "%.*s", (int)(end - start), start);
		}
	}
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	struct rpchandler_history *history = cookie;
	if (!ctx->path)
		return;

	if (!strncmp(RECORDS_PREFIX, ctx->path, strlen(RECORDS_PREFIX))) {
		for (struct rpchandler_history_records **record =
				 history->facilities->records;
			 record && *record; record++) {
			if (!strcmp((*record)->name, ctx->path + strlen(RECORDS_PREFIX))) {
				rpchandler_dir_result(ctx, &rpchistory_fetch);
				rpchandler_dir_result(ctx, &rpchistory_span);
			}
		}
	} else if (!strncmp(FILES_PREFIX, ctx->path, strlen(FILES_PREFIX))) {
		for (struct rpchandler_history_files **file = history->facilities->files;
			 file && *file; file++) {
			// TODO: files based logging
		}
	} else if (history->has_getlog) {
		for (const char **s = history->signals; s && *s; s++) {
			size_t len = strlen(ctx->path);
			if ((ctx->path[0] == '\0') ||
				(!strncmp(*s, ctx->path, len) &&
					((*s)[len] == '\0' || (*s)[len] == '/'))) {
				/* add getLog method if *s starts with ctx->path and is either
				 * entire ctx->path or whole path until '/' character.
				 */
				rpchandler_dir_result(ctx, &rpchistory_getlog);
				break;
			}
		}
	}
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

static void rpcfetch_result(struct rpchandler_history_records *record,
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
		record->pack_record(record->cookie, i, pack, rpchandler_obstack(ctx));
	}

	cp_pack_container_end(pack);
	rpchandler_msg_send_response(ctx, pack);
}

static enum rpchandler_msg_res handle_records_history(
	struct rpchandler_msg *ctx, struct rpchandler_history_records *record) {
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
			rpcfetch_result(record, ctx, start, num);

		return RPCHANDLER_MSG_DONE;
	} else if (!strcmp(ctx->meta.method, "span") &&
		ctx->meta.access >= RPCACCESS_SERVICE) {
		if (rpchandler_msg_valid_nullparam(ctx)) {
			uint64_t min, max, span;
			if (record->get_index_range(record->cookie, &min, &max, &span)) {
				rpcspan_result(ctx, min, max, span);
			} else
				rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM,
					"Could not retrieve information from the log.");
		}
		return RPCHANDLER_MSG_DONE;
	}
	return RPCHANDLER_MSG_SKIP;
}

static enum rpchandler_msg_res handle_files_history(void) {
	// TODO: files based logging
	return RPCHANDLER_MSG_SKIP;
}

static void rpcgetlog_result(struct rpchandler_history_facilities *facilities,
	struct rpchistory_getlog_request *request, struct rpchandler_msg *ctx,
	const char *path) {
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_list_begin(pack);
	if (facilities->pack_getlog(
			facilities, request, pack, rpchandler_obstack(ctx), path)) {
		cp_pack_container_end(pack);
		rpchandler_msg_send_response(ctx, pack);
	} else {
		rpchandler_msg_drop(ctx);
		rpchandler_msg_send_error(ctx, RPCERR_INTERNAL_ERR,
			"Time in the logs is not valid, not possible to provide records.");
	}
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_history *history = cookie;
	if (ctx->meta.type != RPCMSG_T_REQUEST)
		return RPCHANDLER_MSG_SKIP;

	if (history->has_records &&
		!strncmp(ctx->meta.path, RECORDS_PREFIX, strlen(RECORDS_PREFIX))) {
		for (struct rpchandler_history_records **record =
				 history->facilities->records;
			 record && *record; record++) {
			if (!strcmp(ctx->meta.path + strlen(RECORDS_PREFIX), (*record)->name))
				return handle_records_history(ctx, *record);
		}
	} else if (history->has_files &&
		!strncmp(ctx->meta.path, FILES_PREFIX, strlen(FILES_PREFIX))) {
		for (struct rpchandler_history_files **file = history->facilities->files;
			 file && *file; file++) {
			if (!strcmp(ctx->meta.path + strlen(FILES_PREFIX), (*file)->name))
				return handle_files_history();
		}
	} else if (!strcmp(ctx->meta.method, "getLog") && history->has_getlog) {
		for (const char **s = history->signals; s && *s; s++) {
			if ((ctx->meta.path[0] != '\0') &&
				(strncmp(*s, ctx->meta.path, strlen(ctx->meta.path)) ||
					!((*s)[strlen(ctx->meta.path)] == '\0' ||
						(*s)[strlen(ctx->meta.path)] == '/')))
				continue;

			struct rpchistory_getlog_request *request =
				rpchistory_getlog_request_unpack(
					ctx->unpack, ctx->item, rpchandler_obstack(ctx));
			if (!rpchandler_msg_valid(ctx))
				return RPCHANDLER_MSG_DONE;
			if (!request)
				rpchandler_msg_send_error(
					ctx, RPCERR_INVALID_PARAM, "{...} expected.");
			else
				rpcgetlog_result(history->facilities, request, ctx, ctx->meta.path);

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

rpchandler_history_t rpchandler_history_new(
	struct rpchandler_history_facilities *facilities, const char **signals) {
	rpchandler_history_t res = malloc(sizeof *res);
	if (!res)
		return NULL;

	res->facilities = facilities;
	res->signals = signals;
	res->has_records = facilities->records;
	res->has_files = facilities->files;
	res->has_getlog = facilities->pack_getlog;

	return res;
}

void rpchandler_history_destroy(rpchandler_history_t history) {
	free(history);
}

struct rpchandler_stage rpchandler_history_stage(rpchandler_history_t history) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = history};
}
