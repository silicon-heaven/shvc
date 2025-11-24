#include <stdarg.h>
#include <string.h>
#include <shv/rpcmsg.h>


static inline bool meta_begin(cp_pack_t pack) {
	cp_pack_meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID);
	return cp_pack_int(pack, 1);
}

[[gnu::nonnull(1, 2, 3)]]
static bool _rpcmsg_pack_request(cp_pack_t pack, const char *path,
	const char *method, const char *uid, int rid) {
	meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID);
	cp_pack_int(pack, rid);
	cp_pack_int(pack, RPCMSG_TAG_SHV_PATH);
	cp_pack_str(pack, path ?: "");
	cp_pack_int(pack, RPCMSG_TAG_METHOD);
	cp_pack_str(pack, method);
	if (uid) {
		cp_pack_int(pack, RPCMSG_TAG_USER_ID);
		cp_pack_str(pack, uid);
	}
	cp_pack_container_end(pack);

	return cp_pack_imap_begin(pack);
}

bool rpcmsg_pack_request(cp_pack_t pack, const char *path, const char *method,
	const char *uid, int rid) {
	_rpcmsg_pack_request(pack, path, method, uid, rid);
	return cp_pack_int(pack, RPCMSG_KEY_PARAM);
}

bool rpcmsg_pack_request_void(cp_pack_t pack, const char *path,
	const char *method, const char *uid, int rid) {
	_rpcmsg_pack_request(pack, path, method, uid, rid);
	return cp_pack_container_end(pack);
}

static bool _rpcmsg_pack_signal(cp_pack_t pack, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access,
	bool repeat) {
	meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_SHV_PATH);
	cp_pack_str(pack, path ?: "");
	/* Note: We always pack signal name for pre-SHV 3.0 compatibility */
	cp_pack_int(pack, RPCMSG_TAG_SIGNAL);
	if (!cp_pack_str(pack, signal))
		return false;
	if (strcmp(source, "get")) {
		cp_pack_int(pack, RPCMSG_TAG_SOURCE);
		cp_pack_str(pack, source);
	}
	if (uid) {
		cp_pack_int(pack, RPCMSG_TAG_USER_ID);
		cp_pack_str(pack, uid);
	}
	if (access != RPCACCESS_READ) {
		cp_pack_int(pack, RPCMSG_TAG_ACCESS_LEVEL);
		cp_pack_int(pack, access);
	}
	if (repeat) {
		cp_pack_int(pack, RPCMSG_TAG_REPEAT);
		cp_pack_bool(pack, true);
	}
	cp_pack_container_end(pack);

	return cp_pack_imap_begin(pack);
}

bool rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *source,
	const char *signal, const char *uid, rpcaccess_t access, bool repeat) {
	_rpcmsg_pack_signal(pack, path, source, signal, uid, access, repeat);
	return cp_pack_int(pack, RPCMSG_KEY_PARAM);
}

bool rpcmsg_pack_signal_void(cp_pack_t pack, const char *path, const char *source,
	const char *signal, const char *uid, rpcaccess_t access, bool repeat) {
	_rpcmsg_pack_signal(pack, path, source, signal, uid, access, repeat);
	return cp_pack_container_end(pack);
}

static bool _pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID);
	cp_pack_int(pack, meta->request_id);
	if (meta->cids_cnt) {
		cp_pack_int(pack, RPCMSG_TAG_CALLER_IDS);
		if (meta->cids_cnt > 1) {
			cp_pack_list_begin(pack);
			for (size_t i = 0; i < meta->cids_cnt; i++)
				cp_pack_int(pack, meta->cids[i]);
			cp_pack_container_end(pack);
		} else
			cp_pack_int(pack, meta->cids[0]);
	}
	cp_pack_container_end(pack);

	return cp_pack_imap_begin(pack);
}

bool rpcmsg_pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	_pack_response(pack, meta);
	return cp_pack_int(pack, RPCMSG_KEY_RESULT);
}

bool rpcmsg_pack_response_void(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	_pack_response(pack, meta);
	return cp_pack_container_end(pack);
}

bool rpcmsg_pack_error(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errnum, const char *msg) {
	_pack_response(pack, meta);
	cp_pack_int(pack, RPCMSG_KEY_ERROR);
	rpcerror_pack(pack, errnum, msg);
	return cp_pack_container_end(pack);
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
	_pack_response(pack, meta);
	if (!cp_pack_int(pack, RPCMSG_KEY_ERROR))
		return false;

	va_list cargs;
	// Bug: https://github.com/llvm/llvm-project/issues/40656
	va_copy(cargs, args); // NOLINT(clang-analyzer-valist.Uninitialized)
	int siz = vsnprintf(NULL, 0, fmt, cargs);
	va_end(cargs);
	char msg[siz + 1];
	assert(vsnprintf(msg, siz + 1, fmt, args) == siz);
	rpcerror_pack(pack, errnum, msg);

	return cp_pack_container_end(pack);
}

[[gnu::nonnull]]
static bool _rpcmsg_pack_meta(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	meta_begin(pack);
	bool is_rre = meta->type == RPCMSG_T_REQUEST ||
		meta->type == RPCMSG_T_RESPONSE || meta->type == RPCMSG_T_ERROR;
	bool is_rs = meta->type == RPCMSG_T_REQUEST || meta->type == RPCMSG_T_SIGNAL;
	if (is_rre) {
		cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID);
		cp_pack_int(pack, meta->request_id);
	}
	if (is_rs && meta->path && *meta->path != '\0') {
		cp_pack_int(pack, RPCMSG_TAG_SHV_PATH);
		cp_pack_str(pack, meta->path);
	}
	if (is_rs) {
		/* This also covers RPCMSG_TAG_SIGNAL */
		if (!meta->method || *meta->method == '\0')
			return false; /* Invalid meta */
		cp_pack_int(pack, RPCMSG_TAG_METHOD);
		cp_pack_str(pack, meta->method);
	}
	if (is_rre && meta->cids_cnt > 0) {
		cp_pack_int(pack, RPCMSG_TAG_CALLER_IDS);
		if (meta->cids_cnt > 1) {
			cp_pack_list_begin(pack);
			for (size_t i = 0; i < meta->cids_cnt; i++)
				cp_pack_int(pack, meta->cids[i]);
			cp_pack_container_end(pack);
		} else
			cp_pack_int(pack, meta->cids[0]);
	}
	// TODO RPCMSG_TAG_ACCESS for copatibility with clients using that
	if (is_rs && meta->user_id) {
		cp_pack_int(pack, RPCMSG_TAG_USER_ID);
		cp_pack_str(pack, meta->user_id);
	}
	if (is_rs) {
		cp_pack_int(pack, RPCMSG_TAG_ACCESS_LEVEL);
		cp_pack_int(pack, meta->access);
	}
	if (meta->type == RPCMSG_T_SIGNAL) {
		cp_pack_int(pack, RPCMSG_TAG_SOURCE);
		cp_pack_str(pack, meta->source);
	}
	if (meta->type == RPCMSG_T_SIGNAL && meta->repeat) {
		cp_pack_int(pack, RPCMSG_TAG_REPEAT);
		cp_pack_bool(pack, true);
	}
	for (struct rpcmsg_meta_extra *extra = meta->extra; extra; extra = extra->next) {
		// TODO copy additional extra fields
	}
	cp_pack_container_end(pack);

	return cp_pack_imap_begin(pack);
}

bool rpcmsg_pack_meta(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	_rpcmsg_pack_meta(pack, meta);
	if (meta->type == RPCMSG_T_REQUEST || meta->type == RPCMSG_T_SIGNAL)
		return cp_pack_int(pack, RPCMSG_KEY_PARAM);
	if (meta->type == RPCMSG_T_RESPONSE)
		return cp_pack_int(pack, RPCMSG_KEY_RESULT);
	if (meta->type == RPCMSG_T_ERROR)
		return cp_pack_int(pack, RPCMSG_KEY_ERROR);
	return false;
}

bool rpcmsg_pack_meta_void(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	if (meta->type == RPCMSG_T_ERROR)
		return false; /* Error can't be void */
	_rpcmsg_pack_meta(pack, meta);
	return cp_pack_container_end(pack);
}
