#include <stdarg.h>
#include <string.h>
#include <shv/rpcmsg.h>


static inline bool meta_begin(cp_pack_t pack) {
	cp_pack_meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID);
	return cp_pack_int(pack, 1);
}

__attribute__((nonnull(1, 2, 3))) bool _rpcmsg_pack_request(cp_pack_t pack,
	const char *path, const char *method, const char *uid, int rid) {
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
	const char *source, const char *signal, const char *uid, rpcaccess_t access) {
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
	cp_pack_container_end(pack);

	return cp_pack_imap_begin(pack);
}

bool rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *source,
	const char *signal, const char *uid, rpcaccess_t access) {
	_rpcmsg_pack_signal(pack, path, source, signal, uid, access);
	return cp_pack_int(pack, RPCMSG_KEY_PARAM);
}

bool rpcmsg_pack_signal_void(cp_pack_t pack, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access) {
	_rpcmsg_pack_signal(pack, path, source, signal, uid, access);
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
	va_copy(cargs, args);
	// https://github.com/llvm/llvm-project/issues/40656
	int siz = vsnprintf( // NOLINT(clang-analyzer-valist.Uninitialized)
		NULL, 0, fmt, cargs);
	va_end(cargs);
	char msg[siz + 1];
	assert(vsnprintf(msg, siz + 1, fmt, args) == siz);
	rpcerror_pack(pack, errnum, msg);

	return cp_pack_container_end(pack);
}
