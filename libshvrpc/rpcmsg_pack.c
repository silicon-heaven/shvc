#include <shv/rpcmsg.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

bool _rpcmsg_pack_request(
	cp_pack_t pack, const char *path, const char *method, int rid) {
	G(meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID));
	G(cp_pack_int(pack, rid));
	if (path) {
		G(cp_pack_int(pack, RPCMSG_TAG_SHV_PATH));
		G(cp_pack_str(pack, path));
	}
	G(cp_pack_int(pack, RPCMSG_TAG_METHOD));
	G(cp_pack_str(pack, method));
	G(cp_pack_container_end(pack));

	G(cp_pack_imap_begin(pack));
	return true;
}

bool rpcmsg_pack_request(
	cp_pack_t pack, const char *path, const char *method, int rid) {
	G(_rpcmsg_pack_request(pack, path, method, rid));
	G(cp_pack_int(pack, RPCMSG_KEY_PARAMS));
	return true;
}

bool rpcmsg_pack_request_void(
	cp_pack_t pack, const char *path, const char *method, int rid) {
	G(_rpcmsg_pack_request(pack, path, method, rid));
	G(cp_pack_container_end(pack));
	return true;
}

bool rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *method) {
	G(meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_SHV_PATH));
	if (path) {
		G(cp_pack_str(pack, path));
		G(cp_pack_int(pack, RPCMSG_TAG_METHOD));
	}
	G(cp_pack_str(pack, method));
	G(cp_pack_container_end(pack));

	G(cp_pack_imap_begin(pack));
	G(cp_pack_int(pack, RPCMSG_KEY_PARAMS));
	return true;
}

static bool _pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	G(meta_begin(pack));
	G(cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID));
	G(cp_pack_int(pack, meta->request_id));
	if (meta->cids.siz) {
		G(cp_pack_int(pack, RPCMSG_TAG_CALLER_IDS));
		FILE *f = fmemopen(meta->cids.ptr, meta->cids.siz, "r");
		uint8_t buf[BUFSIZ];
		struct cpitem item = (struct cpitem){.buf = buf, .bufsiz = BUFSIZ};
		while (true) {
			/* This has to be a valid chainpack! */
			assert(chainpack_unpack(f, &item) != -1);
			if (item.type == CPITEM_INVALID)
				break;
			G(cp_pack(pack, &item));
		}
		fclose(f);
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
	enum rpcmsg_error error, const char *msg) {
	G(_pack_response(pack, meta));
	G(cp_pack_int(pack, RPCMSG_KEY_ERROR));
	G(cp_pack_imap_begin(pack));
	G(cp_pack_int(pack, RPCMSG_ERR_KEY_CODE));
	G(cp_pack_int(pack, error));
	G(cp_pack_int(pack, RPCMSG_ERR_KEY_MESSAGE));
	G(cp_pack_str(pack, msg));
	G(cp_pack_container_end(pack));
	G(cp_pack_container_end(pack));
	return true;
}

bool rpcmsg_pack_ferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool res = rpcmsg_pack_vferror(pack, meta, error, fmt, args);
	va_end(args);
	return res;
}

bool rpcmsg_pack_vferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, va_list args) {
	va_list argsdup;
	va_copy(argsdup, args);
	size_t siz = vsnprintf(NULL, 0, fmt, argsdup);
	va_end(argsdup);

	char msg[siz + 1];
	vsnprintf(msg, siz + 1, fmt, args);

	return rpcmsg_pack_error(pack, meta, error, msg);
}
