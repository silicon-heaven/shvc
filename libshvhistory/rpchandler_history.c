#include <stdlib.h>
#include <unistd.h>
#include <shv/rpchandler_history.h>
#include <shv/rpchandler_impl.h>
#include <shv/rpchistory.h>

#define HISTORY_PREFIX ".history/"
#define RECORDS_PREFIX ".records/"
#define FILES_PREFIX ".files/"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

struct rpchandler_history {
	bool has_records;
	bool has_files;
	bool has_getlog;
	bool add_prefix;
	const char **signals;
	struct rpchandler_history_facilities *facilities;
};

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_history *history = cookie;

	int offset = 0;
	if (history->add_prefix) {
		if (!ctx->path || ctx->path[0] == '\0') {
			rpchandler_ls_result(ctx, ".history");
			return;
		} else if (strncmp(ctx->path, ".history", strlen(".history"))) {
			return;
		}
		offset = strlen(HISTORY_PREFIX);
	}

	if ((history->add_prefix && ctx->path && !strcmp(ctx->path, ".history")) ||
		(!history->add_prefix && (!ctx->path || ctx->path[0] == '\0'))) {
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
	} else if (!strcmp(ctx->path + offset, ".records")) {
		for (struct rpchandler_history_records **record =
				 history->facilities->records;
			record && *record; record++)
			rpchandler_ls_result(ctx, (*record)->name);
	} else if (!strcmp(ctx->path + offset, ".files")) {
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
			size_t len = strlen(ctx->path) - offset;
			if (strncmp(*s, ctx->path + offset, len) || (*s)[len] != '/')
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

	int offset = 0;
	if (history->add_prefix) {
		if (strncmp(HISTORY_PREFIX, ctx->path, strlen(HISTORY_PREFIX))) {
			return;
		}
		offset = strlen(HISTORY_PREFIX);
	}

	if (!strncmp(RECORDS_PREFIX, ctx->path + offset, strlen(RECORDS_PREFIX))) {
		for (struct rpchandler_history_records **record =
				 history->facilities->records;
			record && *record; record++) {
			if (!strcmp((*record)->name,
					ctx->path + offset + strlen(RECORDS_PREFIX))) {
				rpchandler_dir_result(ctx, &rpchistory_fetch);
				rpchandler_dir_result(ctx, &rpchistory_span);
			}
		}
	} else if (!strncmp(FILES_PREFIX, ctx->path + offset, strlen(FILES_PREFIX))) {
		for (struct rpchandler_history_files **file = history->facilities->files;
			file && *file; file++) {
			// TODO: files based logging
		}
	} else if (history->has_getlog) {
		for (const char **s = history->signals; s && *s; s++) {
			size_t len = strlen(ctx->path) - offset;
			if ((ctx->path[offset] == '\0') ||
				(!strncmp(*s, ctx->path + offset, len) &&
					((*s)[len] == '\0' || (*s)[len] == '/'))) {
				/* add getLog method if *s starts with ctx->path and is
				 * either entire ctx->path or whole path until '/'
				 * character.
				 */
				rpchandler_dir_result(ctx, &rpchistory_getlog);
				rpchandler_dir_result(ctx, &rpchistory_getsnapshot);
				break;
			}
		}
	}
}

static void rpcspan_result(
	struct rpchandler_msg *ctx, struct rpchistory_span_response *res) {
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_list_begin(pack);
	cp_pack_int(pack, res->min);
	cp_pack_int(pack, res->max);
	cp_pack_int(pack, res->span);
	cp_pack_int(pack, res->newest_time_rec);
	cp_pack_container_end(pack);
	rpchandler_msg_send_response(ctx, pack);
}

static void rpcfetch_result(struct rpchandler_history_records *record,
	struct rpchandler_msg *ctx, int start, int num) {
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_list_begin(pack);
	/* Pack all the records from start to start + num. This function can't fail.
	 * If there are none records to be packed (for any reason), then we just
	 * return an empty list.
	 */
	record->pack_records(record->cookie, start, num, pack, rpchandler_obstack(ctx));
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
			struct rpchistory_span_response res;
			if (record->get_index_range(record->cookie, &res)) {
				rpcspan_result(ctx, &res);
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
	if (facilities->funcs->pack_getlog(
			facilities, request, pack, rpchandler_obstack(ctx), path)) {
		cp_pack_container_end(pack);
		rpchandler_msg_send_response(ctx, pack);
	} else {
		rpchandler_msg_drop(ctx);
		rpchandler_msg_send_error(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"Time in the logs is not valid, not possible to provide records.");
	}
}

static void rpcgetsnapshot_result(struct rpchandler_history_facilities *facilities,
	struct rpchistory_getsnapshot_request *request, struct rpchandler_msg *ctx,
	const char *path) {
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_list_begin(pack);
	if (facilities->funcs->pack_getsnapshot(
			facilities, request, pack, rpchandler_obstack(ctx), path)) {
		cp_pack_container_end(pack);
		rpchandler_msg_send_response(ctx, pack);
	} else {
		rpchandler_msg_drop(ctx);
		rpchandler_msg_send_error(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"Time in the logs is not valid, not possible to provide records.");
	}
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_history *history = cookie;
	if (ctx->meta.type != RPCMSG_T_REQUEST)
		return RPCHANDLER_MSG_SKIP;

	int offset = 0;
	if (history->add_prefix) {
		if (strncmp(HISTORY_PREFIX, ctx->meta.path, strlen(HISTORY_PREFIX))) {
			return RPCHANDLER_MSG_SKIP;
		}
		offset = strlen(HISTORY_PREFIX);
	}

	if (history->has_records &&
		!strncmp(ctx->meta.path + offset, RECORDS_PREFIX, strlen(RECORDS_PREFIX))) {
		for (struct rpchandler_history_records **record =
				 history->facilities->records;
			record && *record; record++) {
			if (!strcmp(ctx->meta.path + offset + strlen(RECORDS_PREFIX),
					(*record)->name))
				return handle_records_history(ctx, *record);
		}
	} else if (history->has_files &&
		!strncmp(ctx->meta.path + offset, FILES_PREFIX, strlen(FILES_PREFIX))) {
		for (struct rpchandler_history_files **file = history->facilities->files;
			file && *file; file++) {
			if (!strcmp(ctx->meta.path + offset + strlen(FILES_PREFIX),
					(*file)->name))
				return handle_files_history();
		}
	} else if (!strcmp(ctx->meta.method, "getLog") && history->has_getlog) {
		for (const char **s = history->signals; s && *s; s++) {
			if ((ctx->meta.path[offset] != '\0') &&
				(strncmp(*s, ctx->meta.path + offset, strlen(ctx->meta.path)) ||
					!((*s)[strlen(ctx->meta.path) - offset] == '\0' ||
						(*s)[strlen(ctx->meta.path) - offset] == '/')))
				continue;

			struct rpchistory_getlog_request *request =
				rpchistory_getlog_request_unpack(
					ctx->unpack, ctx->item, rpchandler_obstack(ctx));
			if (!rpchandler_msg_valid(ctx))
				return RPCHANDLER_MSG_DONE;
			if (!request)
				rpchandler_msg_send_error(
					ctx, RPCERR_INVALID_PARAM, "!getLogP expected.");
			else
				rpcgetlog_result(history->facilities, request, ctx, ctx->meta.path);

			return RPCHANDLER_MSG_DONE;
		}
	} else if (!strcmp(ctx->meta.method, "getSnapshot") && history->has_getlog) {
		for (const char **s = history->signals; s && *s; s++) {
			if ((ctx->meta.path[offset] != '\0') &&
				(strncmp(*s, ctx->meta.path + offset, strlen(ctx->meta.path)) ||
					!((*s)[strlen(ctx->meta.path) - offset] == '\0' ||
						(*s)[strlen(ctx->meta.path) - offset] == '/')))
				continue;

			struct rpchistory_getsnapshot_request *request =
				rpchistory_getsnapshot_request_unpack(
					ctx->unpack, ctx->item, rpchandler_obstack(ctx));
			if (!rpchandler_msg_valid(ctx))
				return RPCHANDLER_MSG_DONE;
			if (!request)
				rpchandler_msg_send_error(
					ctx, RPCERR_INVALID_PARAM, "!getSnapshotP expected.");
			else
				rpcgetsnapshot_result(
					history->facilities, request, ctx, ctx->meta.path);

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
	struct rpchandler_history_facilities *facilities, bool add_prefix,
	const char **signals) {
	rpchandler_history_t res = malloc(sizeof *res);
	if (!res)
		return NULL;

	res->facilities = facilities;
	res->signals = signals;
	res->has_records = facilities->records;
	res->has_files = facilities->files;
	res->has_getlog = facilities->funcs && facilities->funcs->pack_getlog &&
		facilities->funcs->pack_getsnapshot;
	res->add_prefix = add_prefix;

	return res;
}

void rpchandler_history_destroy(rpchandler_history_t history) {
	free(history);
}

struct rpchandler_stage rpchandler_history_stage(rpchandler_history_t history) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = history};
}
