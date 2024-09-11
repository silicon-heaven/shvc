#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <shv/crc32.h>
#include <shv/rpcfile.h>
#include <shv/rpchandler_file.h>
#include <shv/rpchandler_impl.h>
#include <shv/sha1.h>

#include "shvc_config.h"

#include "file_method.gperf.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

struct rpchandler_file_repr {
	struct rpcfile_stat_s stat;
	struct rpchandler_file_list *list;
};

struct rpchandler_file {
	int num;
	struct rpchandler_file_repr *file;
	struct rpchandler_file_cb *cb;
};

static void update_cached_stats(const char *path, struct rpcfile_stat_s *file_stat) {
	struct stat st;
	stat(path, &st);
	file_stat->type = RPCFILE_STAT_TYPE_REGULAR;
	file_stat->size = st.st_size;
	file_stat->page_size = st.st_blksize;
	file_stat->access_time = cptmtodt(*gmtime(&st.st_atime));
	file_stat->mod_time = cptmtodt(*gmtime(&st.st_mtime));
}

static bool list_files(rpchandler_file_t handler) {
	struct rpchandler_file_list *list;
	int num;

	if (!handler->cb->ls || !(list = handler->cb->ls(&num)))
		return false;

	handler->file = calloc(num, sizeof *handler->file);
	if (!handler->file) {
		free(list);
		return false;
	}

	handler->num = num;
	for (int i = 0; i < num; i++) {
		struct rpchandler_file_repr *file = handler->file + i;
		file->list = list + i;
		update_cached_stats(file->list->file_path, &file->stat);
		file->stat.max_write = SHVC_FILE_MAXWRITE;
		if (file->list->access & RPCFILE_ACCESS_READ)
			file->list->access |= RPCFILE_ACCESS_VALIDATION;
		if (file->list->access & RPCFILE_ACCESS_TRUNCATE)
			file->list->access |= RPCFILE_ACCESS_WRITE;
	}

	return true;
}

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_file *handler = cookie;
	char *l;
	char *path;
	const char *name;
	bool free_path = false;

	for (int i = 0; i < handler->num; i++) {
		struct rpchandler_file_repr *file = handler->file + i;
		l = strrchr(file->list->shv_path, '/');
		if (!l) {
			name = file->list->shv_path;
			path = "";
		} else {
			free_path = true;
			name = l + 1;
			path = malloc(strlen(file->list->shv_path) - strlen(l));
			strncpy(path, file->list->shv_path,
				strlen(file->list->shv_path) - strlen(l));
		}
		if (ctx->path && !strcmp(ctx->path, path))
			rpchandler_ls_result(ctx, name);
		if (free_path)
			free(path);
	}
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	struct rpchandler_file *handler = cookie;

	if (ctx->name) { /* Faster match against gperf */
		const struct gperf_rpchandler_file_method_match *match =
			gperf_rpchandler_file_method(ctx->name, strlen(ctx->name));
		if (match)
			for (int i = 0; i < handler->num; i++) {
				struct rpchandler_file_repr *file = handler->file + i;
				if (ctx->path && !strcmp(ctx->path, file->list->shv_path)) {
					if (match->method == M_STAT || match->method == M_SIZE ||
						(match->method == M_WRITE &&
							file->list->access & RPCFILE_ACCESS_WRITE) ||
						(match->method == M_TRUNCATE &&
							file->list->access & RPCFILE_ACCESS_TRUNCATE) ||
						(match->method == M_READ &&
							file->list->access & RPCFILE_ACCESS_READ) ||
						(match->method == M_APPEND &&
							file->list->access & RPCFILE_ACCESS_APPEND) ||
						((match->method == M_CRC || match->method == M_SHA1) &&
							file->list->access & RPCFILE_ACCESS_VALIDATION))
						rpchandler_dir_exists(ctx);
					break;
				}
			}
		return;
	}

	for (int i = 0; i < handler->num; i++) {
		struct rpchandler_file_repr *file = handler->file + i;
		if (ctx->path && !strcmp(ctx->path, file->list->shv_path)) {
			if (file->list->access & RPCFILE_ACCESS_VALIDATION) {
				rpchandler_dir_result(ctx, &rpcfile_crc);
				rpchandler_dir_result(ctx, &rpcfile_sha1);
			}
			if (file->list->access & RPCFILE_ACCESS_READ)
				rpchandler_dir_result(ctx, &rpcfile_read);
			if (file->list->access & RPCFILE_ACCESS_WRITE)
				rpchandler_dir_result(ctx, &rpcfile_write);
			if (file->list->access & RPCFILE_ACCESS_TRUNCATE)
				rpchandler_dir_result(ctx, &rpcfile_truncate);
			if (file->list->access & RPCFILE_ACCESS_APPEND)
				rpchandler_dir_result(ctx, &rpcfile_append);

			rpchandler_dir_result(ctx, &rpcfile_stat);
			rpchandler_dir_result(ctx, &rpcfile_size);
			break;
		}
	}
}

static void rpc_handle_write(struct rpchandler_msg *ctx, const char *path) {
	uint8_t buf[SHVC_FILE_MAXWRITE];
	size_t offset;

	/* The message should be packed as [Int, Blob] */
	if ((cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_LIST) ||
		!cp_unpack_int(ctx->unpack, ctx->item, offset) ||
		cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_BLOB) {
		rpchandler_msg_valid(ctx);
		rpchandler_msg_send_error(
			ctx, RPCERR_INVALID_PARAM, "[Int, Blob] expected.");
		return;
	}

	if (ctx->item->as.Blob.eoff > SHVC_FILE_MAXWRITE) {
		rpchandler_msg_valid(ctx);
		rpchandler_msg_send_error(
			ctx, RPCERR_INVALID_PARAM, "Write exceeds maximum allowed size.");
		return;
	}

	cp_unpack_memcpy(ctx->unpack, ctx->item, buf, SHVC_FILE_MAXWRITE);

	if (!rpchandler_msg_valid(ctx))
		return;

	int fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not open the file.");
		return;
	}

	lseek(fd, offset, SEEK_SET);
	int ret = write(fd, (void *)buf, ctx->item->as.Blob.len);
	if (ret != ctx->item->as.Blob.len) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not write the file.");
		close(fd);
		return;
	}

	rpchandler_msg_send_response_void(ctx);
	close(fd);
	return;
}

static void rpc_handle_append(struct rpchandler_msg *ctx, const char *path) {
	uint8_t buf[SHVC_FILE_MAXWRITE];
	cp_unpack(ctx->unpack, ctx->item);
	if (ctx->item->type != CPITEM_BLOB) {
		rpchandler_msg_valid(ctx);
		rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM, "Blob expected.");
		return;
	}

	if (ctx->item->as.Blob.eoff > SHVC_FILE_MAXWRITE) {
		rpchandler_msg_valid(ctx);
		rpchandler_msg_send_error(
			ctx, RPCERR_INVALID_PARAM, "Write exceeds maximum allowed size.");
		return;
	}

	cp_unpack_memcpy(ctx->unpack, ctx->item, buf, SHVC_FILE_MAXWRITE);

	if (!rpchandler_msg_valid(ctx))
		return;

	int fd = open(path, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not open the file.");
		return;
	}

	int ret = write(fd, (void *)buf, ctx->item->as.Blob.len);
	if (ret != ctx->item->as.Blob.len) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not write the file.");
		close(fd);
		return;
	}

	rpchandler_msg_send_response_void(ctx);
	close(fd);
	return;
}

static bool rpcfile_unpack_validation(
	struct rpchandler_msg *ctx, const char *path, size_t *offset, size_t *len) {
	struct stat st;
	*offset = 0;
	*len = 0;

	stat(path, &st);
	cp_unpack(ctx->unpack, ctx->item);
	if (ctx->item->type == CPITEM_NULL)
		*len = st.st_size;
	else if (ctx->item->type == CPITEM_LIST) {
		if (!cp_unpack_int(ctx->unpack, ctx->item, *offset)) {
			rpchandler_msg_valid(ctx);
			rpchandler_msg_send_error(
				ctx, RPCERR_INTERNAL_ERR, "Null | [Int, Int | Null] expected.");
			return false;
		}
		cp_unpack_type(ctx->unpack, ctx->item);
		if (ctx->item->type == CPITEM_INT)
			*len = *offset + ctx->item->as.Int <= st.st_size
				? ctx->item->as.Int
				: st.st_size - *offset;
		else if (ctx->item->type == CPITEM_NULL)
			*len = st.st_size - *offset;
		else {
			rpchandler_msg_valid(ctx);
			rpchandler_msg_send_error(
				ctx, RPCERR_INTERNAL_ERR, "Null | [Int, Int | Null] expected.");
			return false;
		}
	} else {
		rpchandler_msg_valid(ctx);
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Null | [Int, Int | Null] expected.");
		return false;
	}

	return rpchandler_msg_valid(ctx);
}

static void rpc_handle_crc(struct rpchandler_msg *ctx, const char *path) {
	char buf[SHVC_FILE_MAXWRITE];
	size_t offset = 0;
	size_t len = 0;
	crc32_t crc = crc32_init();

	if (!rpcfile_unpack_validation(ctx, path, &offset, &len))
		return;

	int fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not open the file.");
		return;
	}

	lseek(fd, offset, SEEK_SET);
	size_t to_read;
	do {
		/* Do not read the entire file at once to avoid large buffering. We
		 * may use the limitation set by SHVC_FILE_MAXWRITE option here.
		 */
		to_read = len < SHVC_FILE_MAXWRITE ? len : SHVC_FILE_MAXWRITE;
		int ret = read(fd, (void *)buf, to_read);
		if (ret < 0) {
			close(fd);
			rpchandler_msg_send_error(
				ctx, RPCERR_INTERNAL_ERR, "Failed reading the file.");
			return;
		}
		crc = crc32_update(crc, (uint8_t *)buf, ret);
		len -= ret;
	} while (len > 0);

	close(fd);
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_uint(pack, crc32_finalize(crc));
	rpchandler_msg_send_response(ctx, pack);
}

static void rpc_handle_sha1(struct rpchandler_msg *ctx, const char *path) {
	char buf[SHVC_FILE_MAXWRITE];
	size_t offset = 0;
	size_t len = 0;
	sha1ctx_t sha_ctx = sha1_new();
	uint8_t sha[SHA1_SIZ];

	if (!rpcfile_unpack_validation(ctx, path, &offset, &len))
		return;

	int fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not open the file.");
		return;
	}

	lseek(fd, offset, SEEK_SET);
	size_t to_read;
	do {
		/* Do not read the entire file at once to avoid large buffering. We
		 * may use the limitation set by SHVC_FILE_MAXWRITE option here.
		 */
		to_read = len < SHVC_FILE_MAXWRITE ? len : SHVC_FILE_MAXWRITE;
		int ret = read(fd, (void *)buf, to_read);
		if (ret < 0) {
			close(fd);
			sha1_destroy(sha_ctx);
			rpchandler_msg_send_error(
				ctx, RPCERR_INTERNAL_ERR, "Failed reading the file.");
			return;
		}
		sha1_update(sha_ctx, (uint8_t *)buf, ret);
		len -= ret;
	} while (len > 0);

	sha1_digest(sha_ctx, sha);

	close(fd);
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_blob(pack, sha, SHA1_SIZ);
	rpchandler_msg_send_response(ctx, pack);
}

static void rpc_handle_read(struct rpchandler_msg *ctx, const char *path) {
	struct stat st;
	uint8_t buf[256];
	size_t offset;
	size_t len;

	if (cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_LIST ||
		!cp_unpack_int(ctx->unpack, ctx->item, offset) ||
		!cp_unpack_int(ctx->unpack, ctx->item, len)) {
		rpchandler_msg_valid(ctx);
		rpchandler_msg_send_error(
			ctx, RPCERR_INVALID_PARAM, "[Int, Int] expected.");
		return;
	}

	if (!rpchandler_msg_valid(ctx))
		return;

	stat(path, &st);
	len = offset + len > st.st_size ? st.st_size - offset : len;

	int fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		rpchandler_msg_send_error(
			ctx, RPCERR_INTERNAL_ERR, "Could not open the file.");
		return;
	}

	cp_pack_t pack = rpchandler_msg_new_response(ctx);

	cp_pack_blob_size(pack, ctx->item, len);
	lseek(fd, offset, SEEK_SET);
	size_t to_read;
	do {
		to_read = len < 256 ? len : 256;
		int ret = read(fd, buf, to_read);
		if (ret < 0) {
			close(fd);
			rpchandler_msg_send_error(
				ctx, RPCERR_INTERNAL_ERR, "Failed reading the file.");
			return;
		}
		cp_pack_blob_data(pack, ctx->item, buf, ret);
		len -= ret;
	} while (len > 0);

	close(fd);
	rpchandler_msg_send_response(ctx, pack);
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_file *handler = cookie;
	if (ctx->meta.type != RPCMSG_T_REQUEST)
		return RPCHANDLER_MSG_SKIP;

	for (int i = 0; i < handler->num; i++) {
		struct rpchandler_file_repr *file = handler->file + i;

		if (handler->cb->update_paths) {
			if (handler->cb->update_paths(
					&file->list->file_path, file->list->shv_path) < 0) {
				rpchandler_msg_valid(ctx);
				rpchandler_msg_send_error(
					ctx, RPCERR_INTERNAL_ERR, "Could not obtain file paths.");
				return RPCHANDLER_MSG_DONE;
			}
		}

		if (!ctx->meta.path || strcmp(ctx->meta.path, file->list->shv_path))
			continue;
		const struct gperf_rpchandler_file_method_match *match =
			gperf_rpchandler_file_method(
				ctx->meta.method, strlen(ctx->meta.method));
		if (match != NULL)
			switch (match->method) {
				case M_STAT: {
					if (ctx->meta.access < RPCACCESS_READ)
						break;
					if (rpchandler_msg_valid_nullparam(ctx)) {
						update_cached_stats(file->list->file_path, &file->stat);
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						rpcfile_stat_pack(pack, &file->stat);
						rpchandler_msg_send_response(ctx, pack);
					}
					return RPCHANDLER_MSG_DONE;
				}
				case M_SIZE: {
					if (ctx->meta.access < RPCACCESS_READ)
						break;
					if (rpchandler_msg_valid_nullparam(ctx)) {
						update_cached_stats(file->list->file_path, &file->stat);
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						cp_pack_int(pack, file->stat.size);
						cp_pack_container_end(pack);
						rpchandler_msg_send(ctx);
					}
					return RPCHANDLER_MSG_DONE;
				}
				case M_WRITE:
					if (!(file->list->access & RPCFILE_ACCESS_WRITE) ||
						ctx->meta.access < RPCACCESS_WRITE)
						break;
					rpc_handle_write(ctx, file->list->file_path);
					return RPCHANDLER_MSG_DONE;
				case M_TRUNCATE:
					if (!(file->list->access & RPCFILE_ACCESS_TRUNCATE) ||
						ctx->meta.access < RPCACCESS_WRITE)
						break;
					size_t length;
					bool valid = cp_unpack_int(ctx->unpack, ctx->item, length);
					if (rpchandler_msg_valid(ctx)) {
						if (valid) {
							if (truncate(file->list->file_path, length) < 0)
								rpchandler_msg_send_error(ctx,
									RPCERR_INTERNAL_ERR, "truncate failed.");
							else
								rpchandler_msg_send_response_void(ctx);
						} else
							rpchandler_msg_send_error(
								ctx, RPCERR_INVALID_PARAM, "Int expected.");
					}
					return RPCHANDLER_MSG_DONE;
				case M_READ:
					if (!(file->list->access & RPCFILE_ACCESS_READ) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_read(ctx, file->list->file_path);
					return RPCHANDLER_MSG_DONE;
				case M_APPEND:
					if (!(file->list->access & RPCFILE_ACCESS_APPEND) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_append(ctx, file->list->file_path);
					return RPCHANDLER_MSG_DONE;
				case M_CRC:
					if (!(file->list->access & RPCFILE_ACCESS_VALIDATION) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_crc(ctx, file->list->file_path);
					return RPCHANDLER_MSG_DONE;
				case M_SHA1:
					if (!(file->list->access & RPCFILE_ACCESS_VALIDATION) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_sha1(ctx, file->list->file_path);
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

rpchandler_file_t rpchandler_file_new(struct rpchandler_file_cb *cb) {
	rpchandler_file_t handler = calloc(1, sizeof *handler);
	if (!handler) {
		return NULL;
	}

	handler->cb = cb;
	if (!list_files(handler)) {
		free(handler);
		return NULL;
	}

	return handler;
}

void rpchandler_file_destroy(rpchandler_file_t handler) {
	free(handler->file->list);
	free(handler->file);
	free(handler);
}

struct rpchandler_stage rpchandler_file_stage(rpchandler_file_t handler) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = handler};
}
