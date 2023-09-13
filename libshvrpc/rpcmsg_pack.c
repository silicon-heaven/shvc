#include <shv/rpcmsg.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define RES(V) \
	do { \
		size_t __res = V; \
		if (__res == 0) \
			return 0; \
		res += __res; \
	} while (false)


static inline size_t meta_begin(cp_pack_t pack) {
	size_t res = 0;
	RES(cp_pack_meta_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID));
	RES(cp_pack_int(pack, 1));
	return res;
}


size_t rpcmsg_pack_request(
	cp_pack_t pack, const char *path, const char *method, int rid) {
	size_t res = 0;
	RES(meta_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID));
	RES(cp_pack_int(pack, rid));
	if (path) {
		RES(cp_pack_int(pack, RPCMSG_TAG_SHV_PATH));
		RES(cp_pack_str(pack, path));
	}
	RES(cp_pack_int(pack, RPCMSG_TAG_METHOD));
	RES(cp_pack_str(pack, method));
	RES(cp_pack_container_end(pack));

	RES(cp_pack_imap_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_KEY_PARAMS));
	return res;
}

size_t rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *method) {
	size_t res = 0;
	RES(meta_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_TAG_SHV_PATH));
	if (path) {
		RES(cp_pack_str(pack, path));
		RES(cp_pack_int(pack, RPCMSG_TAG_METHOD));
	}
	RES(cp_pack_str(pack, method));
	RES(cp_pack_container_end(pack));

	RES(cp_pack_imap_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_KEY_PARAMS));
	return res;
}

static size_t _pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	size_t res = 0;
	RES(meta_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID));
	RES(cp_pack_int(pack, meta->request_id));
	if (meta->cids.siz) {
		RES(cp_pack_int(pack, RPCMSG_TAG_CALLER_IDS));
		FILE *f = fmemopen(meta->cids.ptr, meta->cids.siz, "r");
		uint8_t buf[BUFSIZ];
		struct cpitem item = (struct cpitem){.buf = buf, .bufsiz = BUFSIZ};
		while (true) {
			/* This has to be a valid chainpack! */
			assert(chainpack_unpack(f, &item) != -1);
			if (item.type == CPITEM_INVALID)
				break;
			RES(cp_pack(pack, &item));
		}
		fclose(f);
	}
	RES(cp_pack_container_end(pack));

	RES(cp_pack_imap_begin(pack));
	return res;
}

size_t rpcmsg_pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta) {
	size_t res = 0;
	RES(_pack_response(pack, meta));
	RES(cp_pack_int(pack, RPCMSG_KEY_RESULT));
	return res;
}

size_t rpcmsg_pack_error(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *msg) {
	size_t res = 0;
	RES(_pack_response(pack, meta));
	RES(cp_pack_int(pack, RPCMSG_KEY_ERROR));
	RES(cp_pack_imap_begin(pack));
	RES(cp_pack_int(pack, RPCMSG_ERR_KEY_CODE));
	RES(cp_pack_int(pack, error));
	RES(cp_pack_int(pack, RPCMSG_ERR_KEY_MESSAGE));
	RES(cp_pack_str(pack, msg));
	RES(cp_pack_container_end(pack));
	RES(cp_pack_container_end(pack));
	return res;
}

size_t rpcmsg_pack_ferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	size_t res = rpcmsg_pack_vferror(pack, meta, error, fmt, args);
	va_end(args);
	return res;
}

size_t rpcmsg_pack_vferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, va_list args) {
	va_list argsdup;
	va_copy(argsdup, args);
	size_t siz = vsnprintf(NULL, 0, fmt, argsdup);
	va_end(argsdup);

	char msg[siz + 1];
	vsnprintf(msg, siz + 1, fmt, args);

	return rpcmsg_pack_error(pack, meta, error, msg);
}
