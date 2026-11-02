#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <shv/crc32.h>
#include <shv/rpcfile.h>
#include <shv/rpchandler_file.h>
#include <shv/rpchandler_impl.h>
#include <shv/sha1.h>

#include "shvc_config.h"

#include "file_method.gperf.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

struct rpchandler_file {
	const struct rpchandler_file_node **entries;
};

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_file *handler = cookie;
	size_t len = strlen(ctx->path);
	for (const struct rpchandler_file_node **e = handler->entries; *e; e++) {
		const struct rpchandler_file_node *entry = *e;

		if (ctx->name && entry->isdir && !strcmp(ctx->path, entry->shv_path)) {
			if (strchr(ctx->name, '/'))
				return;

			char fullpath[strlen(entry->file_path) + strlen(ctx->name) + 2];
			sprintf(fullpath, "%s/%s", entry->file_path, ctx->name);
			struct stat sb;
			if (strstr(fullpath, "..") == NULL && stat(fullpath, &sb) == 0 &&
				S_ISREG(sb.st_mode)) {
				rpchandler_ls_exists(ctx);
				return;
			}
		}

		if (*ctx->path != '\0') {
			if (strncmp(entry->shv_path, ctx->path, len))
				continue;

			/* Check if this is full path or just subpath. Full path would
			 * have null byte at the end, subpath will have '/'
			 *
			 * If we have full path then we need to check files in the directory
			 * if the entry is a directory.
			 */
			if (entry->shv_path[len] != '/') {
				if (!entry->isdir)
					continue;

				struct stat sb;
				if (stat(entry->file_path, &sb) != 0 || !S_ISDIR(sb.st_mode))
					continue;

				DIR *d = opendir(entry->file_path);
				if (d == NULL)
					continue;

				struct dirent *dirent;
				while ((dirent = readdir(d))) {
					if (dirent->d_type != DT_REG)
						continue;

					rpchandler_ls_result_const(ctx, dirent->d_name);
				}

				closedir(d);
				continue;
			}
		}

		const char *start = entry->shv_path + len;
		if (*start == '/') {
			start++;
		}
		const char *end = strchrnul(start, '/');
		rpchandler_ls_result_fmt(ctx, "%.*s", (int)(end - start), start);
	}
}

static bool rpc_dir_has_file(
	const char *path, const struct rpchandler_file_node *entry) {
	char *c = strrchr(path, '/');
	if (!c)
		return false;
	size_t offset = strlen(c);
	if (strncmp(entry->shv_path, path, strlen(path) - offset) ||
		!strcmp(entry->shv_path, path)) {
		return false;
	}
	char fullpath[strlen(entry->file_path) + offset + 2];
	sprintf(fullpath, "%s/%s", entry->file_path, c ? c : "");
	struct stat sb;
	return strstr(fullpath, "..") == NULL && stat(fullpath, &sb) == 0 &&
		S_ISREG(sb.st_mode);
}

static const struct rpchandler_file_node *select_file_node(
	const struct rpchandler_file_node **entries, const char *path) {
	const struct rpchandler_file_node *entry;
	while ((entry = *(entries++))) {
		if (entry->isdir) {
			if (rpc_dir_has_file(path, entry))
				return entry;
		} else if (!strcmp(path, entry->shv_path))
			return entry;
	}
	return NULL;
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	struct rpchandler_file *handler = cookie;

	if (*ctx->path == '\0') {
		return;
	}

	if (ctx->name) { /* Faster match against gperf */
		const struct gperf_rpchandler_file_method_match *match =
			gperf_rpchandler_file_method(ctx->name, strlen(ctx->name));
		if (match) {
			const struct rpchandler_file_node *entry =
				select_file_node(handler->entries, ctx->path);
			if (entry) {
				bool exists = false;
				switch (match->method) {
					case M_STAT:
					case M_SIZE:
						exists = true;
						break;
					case M_WRITE:
						exists = entry->access &
							(RPCFILE_ACCESS_WRITE | RPCFILE_ACCESS_TRUNCATE);
						break;
					case M_TRUNCATE:
						exists = entry->access & RPCFILE_ACCESS_TRUNCATE;
						break;
					case M_READ:
						exists = entry->access & RPCFILE_ACCESS_READ;
						break;
					case M_APPEND:
						exists = entry->access & RPCFILE_ACCESS_APPEND;
						break;
					case M_CRC:
					case M_SHA1:
						exists = entry->access &
							(RPCFILE_ACCESS_VALIDATION | RPCFILE_ACCESS_READ);
						break;
				}
				if (exists)
					rpchandler_dir_exists(ctx);
			}
			return;
		}
	}

	const struct rpchandler_file_node *entry =
		select_file_node(handler->entries, ctx->path);
	if (entry) {
		rpchandler_dir_result(ctx, &rpcfile_stat);
		rpchandler_dir_result(ctx, &rpcfile_size);
		if (entry->access & (RPCFILE_ACCESS_VALIDATION | RPCFILE_ACCESS_READ)) {
			rpchandler_dir_result(ctx, &rpcfile_crc);
			rpchandler_dir_result(ctx, &rpcfile_sha1);
		}
		if (entry->access & RPCFILE_ACCESS_READ)
			rpchandler_dir_result(ctx, &rpcfile_read);
		if (entry->access & (RPCFILE_ACCESS_WRITE | RPCFILE_ACCESS_TRUNCATE))
			rpchandler_dir_result(ctx, &rpcfile_write);
		if (entry->access & RPCFILE_ACCESS_TRUNCATE)
			rpchandler_dir_result(ctx, &rpcfile_truncate);
		if (entry->access & RPCFILE_ACCESS_APPEND)
			rpchandler_dir_result(ctx, &rpcfile_append);
	}
}

static inline void _write_too_large(struct rpchandler_msg *ctx) {
	rpchandler_msg_send_ferror(ctx, RPCERR_INVALID_PARAM,
		"Write exceeds maximum allowed size %d.", SHVC_FILE_MAXWRITE);
}

static inline int _open_file(struct rpchandler_msg *ctx, const char *path) {
	int fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0)
		rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"Could not open the file: %s", strerror(errno));
	return fd;
}

static void rpc_handle_write(struct rpchandler_msg *ctx, const char *path) {
	uint8_t buf[SHVC_FILE_MAXWRITE];
	size_t offset;

	/* The message should be packed as [Int, Blob] */
	if (!rpcmsg_has_value(ctx->item) ||
		cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_LIST ||
		!cp_unpack_int(ctx->unpack, ctx->item, offset) ||
		cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_BLOB) {
		if (rpchandler_msg_valid(ctx))
			rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM, "[i,b] expected.");
		return;
	}

	if (ctx->item->as.Blob.flags & CPBI_F_STREAM &&
		ctx->item->as.Blob.eoff > SHVC_FILE_MAXWRITE)
		return _write_too_large(ctx);

	ssize_t siz = cp_unpack_memcpy(ctx->unpack, ctx->item, buf, SHVC_FILE_MAXWRITE);

	if (siz < 0 || !rpchandler_msg_valid(ctx))
		return;

	if (!(ctx->item->as.Blob.flags & CPBI_F_LAST))
		return _write_too_large(ctx);

	int fd = _open_file(ctx, path);
	if (fd < 0)
		return;

	lseek(fd, offset, SEEK_SET);
	int ret = write(fd, (void *)buf, siz);
	if (ret == siz) {
		rpchandler_msg_send_response_void(ctx);
	} else if (ret < 0) {
		rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"File write failed: %s", strerror(errno));
	} else
		rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"Short file write (%d bytes only)", ret);

	close(fd);
}

static void rpc_handle_append(struct rpchandler_msg *ctx, const char *path) {
	uint8_t buf[SHVC_FILE_MAXWRITE];
	if (!rpcmsg_has_value(ctx->item) ||
		cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_BLOB) {
		if (rpchandler_msg_valid(ctx))
			rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM, "Blob expected.");
		return;
	}

	if (ctx->item->as.Blob.flags & CPBI_F_STREAM &&
		ctx->item->as.Blob.eoff > SHVC_FILE_MAXWRITE)
		return _write_too_large(ctx);

	ssize_t siz = cp_unpack_memcpy(ctx->unpack, ctx->item, buf, SHVC_FILE_MAXWRITE);

	if (siz < 0 || !rpchandler_msg_valid(ctx))
		return;

	if (!(ctx->item->as.Blob.flags & CPBI_F_LAST))
		return _write_too_large(ctx);

	int fd = _open_file(ctx, path);
	if (fd < 0)
		return;

	lseek(fd, 0, SEEK_END);
	int ret = write(fd, (void *)buf, siz);
	if (ret == siz) {
		rpchandler_msg_send_response_void(ctx);
	} else if (ret < 0) {
		rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"File write failed: %s", strerror(errno));
	} else
		rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
			"Short file write (%d bytes only)", ret);

	close(fd);
}

static bool rpcfile_unpack_validation_param(
	struct rpchandler_msg *ctx, size_t *offset, ssize_t *len) {
	*offset = 0;
	*len = -1;
	bool valid = false;

	if (rpcmsg_has_value(ctx->item)) {
		cp_unpack(ctx->unpack, ctx->item);
		if (ctx->item->type == CPITEM_LIST) {
			valid = cp_unpack_int(ctx->unpack, ctx->item, *offset);
			if (valid) {
				ssize_t tmplen;
				if (cp_unpack_int(ctx->unpack, ctx->item, tmplen)) {
					valid = tmplen >= 0 &&
						cp_unpack_type(ctx->unpack, ctx->item) ==
							CPITEM_CONTAINER_END;
					*len = tmplen;
				} else
					valid = ctx->item->type == CPITEM_CONTAINER_END;
			}
		} else if (ctx->item->type == CPITEM_NULL)
			valid = true;
	} else
		valid = true;

	if (!rpchandler_msg_valid(ctx))
		return false;
	if (valid)
		return true;
	rpchandler_msg_send_error(
		ctx, RPCERR_INVALID_PARAM, "'[i(0,),i(0,)|n]|n' expected.");
	return false;
}

static void rpc_handle_crc(struct rpchandler_msg *ctx, const char *path) {
	size_t offset;
	ssize_t len;
	if (!rpcfile_unpack_validation_param(ctx, &offset, &len))
		return;

	int fd = _open_file(ctx, path);
	if (fd < 0)
		return;

	crc32_t crc = crc32_init();
	char buf[SHVC_FILE_MAXWRITE];

	lseek(fd, offset, SEEK_SET);
	size_t to_read;
	do {
		/* Do not read the entire file at once to avoid large buffering. We
		 * may use the limitation set by SHVC_FILE_MAXWRITE option here.
		 */
		to_read = len < SHVC_FILE_MAXWRITE && len >= 0 ? len : SHVC_FILE_MAXWRITE;
		int ret = read(fd, (void *)buf, to_read);
		if (ret == 0)
			break;
		if (ret < 0) {
			rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
				"File read failed: %s", strerror(errno));
			close(fd);
			return;
		}
		crc = crc32_update(crc, (uint8_t *)buf, ret);
		len -= len >= 0 ? ret : 0;
	} while (len > 0 || len == -1);
	close(fd);

	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_uint(pack, crc32_finalize(crc));
	rpchandler_msg_send_response(ctx, pack);
}

static void rpc_handle_sha1(struct rpchandler_msg *ctx, const char *path) {
	size_t offset;
	ssize_t len;
	if (!rpcfile_unpack_validation_param(ctx, &offset, &len))
		return;

	int fd = _open_file(ctx, path);
	if (fd < 0)
		return;

	sha1ctx_t sha_ctx = sha1_new();
	char buf[SHVC_FILE_MAXWRITE];

	lseek(fd, offset, SEEK_SET);
	size_t to_read;
	do {
		/* Do not read the entire file at once to avoid large buffering. We
		 * may use the limitation set by SHVC_FILE_MAXWRITE option here.
		 */
		to_read = len < SHVC_FILE_MAXWRITE && len >= 0 ? len : SHVC_FILE_MAXWRITE;
		int ret = read(fd, (void *)buf, to_read);
		if (ret == 0)
			break;
		if (ret < 0) {
			close(fd);
			sha1_destroy(sha_ctx);
			rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
				"File read failed: %s", strerror(errno));
			return;
		}
		sha1_update(sha_ctx, (uint8_t *)buf, ret);
		len -= len >= 0 ? ret : 0;
	} while (len > 0 || len == -1);
	close(fd);

	uint8_t sha[SHA1_SIZ];
	sha1_digest(sha_ctx, sha);
	sha1_destroy(sha_ctx);
	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	cp_pack_blob(pack, sha, SHA1_SIZ);
	rpchandler_msg_send_response(ctx, pack);
}

static void rpc_handle_read(struct rpchandler_msg *ctx, const char *path) {
	struct stat st;
	uint8_t buf[256];
	size_t offset;
	size_t len;

	if (!rpcmsg_has_value(ctx->item) ||
		cp_unpack_type(ctx->unpack, ctx->item) != CPITEM_LIST ||
		!cp_unpack_int(ctx->unpack, ctx->item, offset) ||
		!cp_unpack_int(ctx->unpack, ctx->item, len)) {
		if (rpchandler_msg_valid(ctx))
			rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM, "[i,i] expected.");
		return;
	}

	if (!rpchandler_msg_valid(ctx))
		return;

	stat(path, &st);
	len = offset + len > st.st_size ? st.st_size - offset : len;

	int fd = _open_file(ctx, path);
	if (fd < 0)
		return;

	cp_pack_t pack = rpchandler_msg_new_response(ctx);

	cp_pack_blob_size(pack, ctx->item, len);
	lseek(fd, offset, SEEK_SET);
	size_t to_read;
	do {
		to_read = len < 256 ? len : 256;
		int ret = read(fd, buf, to_read);
		if (ret < 0) {
			rpchandler_msg_send_ferror(ctx, RPCERR_METHOD_CALL_EXCEPTION,
				"File read failed: %s", strerror(errno));
			close(fd);
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

	for (const struct rpchandler_file_node **e = handler->entries; *e; e++) {
		const struct rpchandler_file_node *entry = *e;
		char *fullpath = (char *)entry->file_path;
		if (entry->isdir) {
			char *c = strrchr(ctx->meta.path, '/');
			size_t offset = c ? strlen(c) : 0;
			if (strncmp(entry->shv_path, ctx->meta.path,
					strlen(ctx->meta.path) - offset) ||
				!strcmp(entry->shv_path, ctx->meta.path)) {
				continue;
			}

			obstack_printf(rpchandler_obstack(ctx), "%s%s%s%c",
				entry->file_path, c ? "/" : "", c ? c : "", '\0');
			fullpath = obstack_finish(rpchandler_obstack(ctx));

			if (strstr(fullpath, "..") || access(fullpath, F_OK) != 0) {
				continue;
			}
		} else if (strcmp(ctx->meta.path, entry->shv_path)) {
			continue;
		}

		const struct gperf_rpchandler_file_method_match *match =
			gperf_rpchandler_file_method(
				ctx->meta.method, strlen(ctx->meta.method));
		if (match != NULL)
			switch (match->method) {
				case M_STAT: {
					if (ctx->meta.access < RPCACCESS_READ)
						break;
					if (rpchandler_msg_valid_nullparam(ctx)) {
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						struct stat st;
						stat(fullpath, &st);
						struct rpcfile_stat_s stat = {0};
						stat.type = RPCFILE_STAT_TYPE_REGULAR;
						stat.size = st.st_size;
						stat.page_size = st.st_blksize;
						stat.access_time = cptmtodt(*gmtime(&st.st_atime));
						stat.mod_time = cptmtodt(*gmtime(&st.st_mtime));
						stat.max_write = SHVC_FILE_MAXWRITE;
						rpcfile_stat_pack(pack, &stat);
						rpchandler_msg_send_response(ctx, pack);
					}
					return RPCHANDLER_MSG_DONE;
				}
				case M_SIZE: {
					if (ctx->meta.access < RPCACCESS_READ)
						break;
					if (rpchandler_msg_valid_nullparam(ctx)) {
						struct stat st;
						stat(fullpath, &st);
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						cp_pack_int(pack, st.st_size);
						cp_pack_container_end(pack);
						rpchandler_msg_send(ctx);
					}
					return RPCHANDLER_MSG_DONE;
				}
				case M_WRITE:
					if (!(entry->access &
							(RPCFILE_ACCESS_WRITE | RPCFILE_ACCESS_TRUNCATE)) ||
						ctx->meta.access < RPCACCESS_WRITE)
						break;
					rpc_handle_write(ctx, fullpath);
					return RPCHANDLER_MSG_DONE;
				case M_TRUNCATE:
					if (!(entry->access & RPCFILE_ACCESS_TRUNCATE) ||
						ctx->meta.access < RPCACCESS_WRITE)
						break;
					size_t length;
					bool valid = cp_unpack_int(ctx->unpack, ctx->item, length);
					if (rpchandler_msg_valid(ctx)) {
						if (valid) {
							if (truncate(fullpath, length) < 0)
								rpchandler_msg_send_error(ctx,
									RPCERR_METHOD_CALL_EXCEPTION,
									"truncate failed.");
							else
								rpchandler_msg_send_response_void(ctx);
						} else
							rpchandler_msg_send_error(
								ctx, RPCERR_INVALID_PARAM, "Int expected.");
					}
					return RPCHANDLER_MSG_DONE;
				case M_READ:
					if (!(entry->access & RPCFILE_ACCESS_READ) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_read(ctx, fullpath);
					return RPCHANDLER_MSG_DONE;
				case M_APPEND:
					if (!(entry->access & RPCFILE_ACCESS_APPEND) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_append(ctx, fullpath);
					return RPCHANDLER_MSG_DONE;
				case M_CRC:
					if (!(entry->access &
							(RPCFILE_ACCESS_VALIDATION | RPCFILE_ACCESS_READ)) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_crc(ctx, fullpath);
					return RPCHANDLER_MSG_DONE;
				case M_SHA1:
					if (!(entry->access &
							(RPCFILE_ACCESS_VALIDATION | RPCFILE_ACCESS_READ)) ||
						ctx->meta.access < RPCACCESS_READ)
						break;
					rpc_handle_sha1(ctx, fullpath);
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

rpchandler_file_t rpchandler_file_new(const struct rpchandler_file_node **entries) {
	for (const struct rpchandler_file_node **e = entries; *e; e++) {
		const struct rpchandler_file_node *entry = *e;
		if (!entry->file_path || strlen(entry->file_path) > PATH_MAX)
			return NULL;

		if (!entry->isdir) {
			struct stat st;
			stat(entry->file_path, &st);
			if (!S_ISREG(st.st_mode))
				return NULL;
		}
	}

	struct rpchandler_file *handler = malloc(sizeof *handler);
	if (!handler) {
		return NULL;
	}

	handler->entries = entries;
	return handler;
}

void rpchandler_file_destroy(rpchandler_file_t handler) {
	free(handler);
}

struct rpchandler_stage rpchandler_file_stage(rpchandler_file_t handler) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = handler};
}
