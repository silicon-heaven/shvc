#include <stdarg.h>
#include <string.h>
#include <shv/rpcmsg.h>

#define G(V) \
	do { \
		if (!(V)) \
			return false; \
	} while (false)


static inline bool meta_begin(cp_pack_t pack) {
	G(cp_pack_meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID));
	G(cp_pack_int(pack, 1));
	return true;
}

__attribute__((nonnull(1, 2, 3))) bool _rpcmsg_pack_request(cp_pack_t pack,
	const char *path, const char *method, const char *uid, int rid) {
	G(meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID));
	G(cp_pack_int(pack, rid));
	G(cp_pack_int(pack, RPCMSG_TAG_SHV_PATH));
	G(cp_pack_str(pack, path ?: ""));
	G(cp_pack_int(pack, RPCMSG_TAG_METHOD));
	G(cp_pack_str(pack, method));
	if (uid) {
		G(cp_pack_int(pack, RPCMSG_TAG_USER_ID));
		G(cp_pack_str(pack, uid));
	}
	G(cp_pack_container_end(pack));

	G(cp_pack_imap_begin(pack));
	return true;
}

bool rpcmsg_pack_request(cp_pack_t pack, const char *path, const char *method,
	const char *uid, int rid) {
	G(_rpcmsg_pack_request(pack, path, method, uid, rid));
	G(cp_pack_int(pack, RPCMSG_KEY_PARAM));
	return true;
}

bool rpcmsg_pack_request_void(cp_pack_t pack, const char *path,
	const char *method, const char *uid, int rid) {
	G(_rpcmsg_pack_request(pack, path, method, uid, rid));
	G(cp_pack_container_end(pack));
	return true;
}

static bool _rpcmsg_pack_signal(cp_pack_t pack, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access) {
	G(meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_SHV_PATH));
	G(cp_pack_str(pack, path ?: ""));
	/* Note: We always pack signal name for pre-SHV 3.0 compatibility */
	G(cp_pack_int(pack, RPCMSG_TAG_SIGNAL));
	G(cp_pack_str(pack, signal));
	if (strcmp(source, "get")) {
		G(cp_pack_int(pack, RPCMSG_TAG_SOURCE));
		G(cp_pack_str(pack, source));
	}
	if (uid) {
		G(cp_pack_int(pack, RPCMSG_TAG_USER_ID));
		G(cp_pack_str(pack, uid));
	}
	if (access != RPCACCESS_READ) {
		G(cp_pack_int(pack, RPCMSG_TAG_ACCESS_LEVEL));
		G(cp_pack_int(pack, access));
	}
	G(cp_pack_container_end(pack));

	G(cp_pack_imap_begin(pack));
	return true;
}

bool rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *source,
	const char *signal, const char *uid, rpcaccess_t access) {
	G(_rpcmsg_pack_signal(pack, path, source, signal, uid, access));
	G(cp_pack_int(pack, RPCMSG_KEY_PARAM));
	return true;
}

bool rpcmsg_pack_signal_void(cp_pack_t pack, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access) {
	G(_rpcmsg_pack_signal(pack, path, source, signal, uid, access));
	G(cp_pack_container_end(pack));
	return true;
}

static bool _pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	G(meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID));
	G(cp_pack_int(pack, meta->request_id));
	if (meta->cids_cnt) {
		G(cp_pack_int(pack, RPCMSG_TAG_CALLER_IDS));
		if (meta->cids_cnt > 1) {
			G(cp_pack_list_begin(pack));
			for (size_t i = 0; i < meta->cids_cnt; i++)
				G(cp_pack_int(pack, meta->cids[i]));
			G(cp_pack_container_end(pack));
		} else
			G(cp_pack_int(pack, meta->cids[0]));
	}
	G(cp_pack_container_end(pack));

	G(cp_pack_imap_begin(pack));
	return true;
}

bool rpcmsg_pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	G(_pack_response(pack, meta));
	G(cp_pack_int(pack, RPCMSG_KEY_RESULT));
	return true;
}

bool rpcmsg_pack_response_void(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	G(_pack_response(pack, meta));
	G(cp_pack_container_end(pack));
	return true;
}

bool rpcmsg_pack_error(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errnum, const char *msg) {
	G(_pack_response(pack, meta));
	G(cp_pack_int(pack, RPCMSG_KEY_ERROR));
	G(rpcerror_pack(pack, errnum, msg));
	G(cp_pack_container_end(pack));
	return true;
}

bool rpcmsg_pack_ferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errnum, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool res = rpcmsg_pack_vferror(pack, meta, errnum, fmt, args);
	va_end(args);
	return res;
}

bool rpcmsg_pack_vferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errnum, const char *fmt, va_list args) {
	G(_pack_response(pack, meta));
	G(cp_pack_int(pack, RPCMSG_KEY_ERROR));

	va_list cargs;
	va_copy(cargs, args);
	// https://github.com/llvm/llvm-project/issues/40656
	int siz = vsnprintf( // NOLINT(clang-analyzer-valist.Uninitialized)
		NULL, 0, fmt, cargs);
	va_end(cargs);
	char msg[siz + 1];
	assert(vsnprintf(msg, siz + 1, fmt, args) == siz);
	G(rpcerror_pack(pack, errnum, msg));

	G(cp_pack_container_end(pack));
	return true;
}
