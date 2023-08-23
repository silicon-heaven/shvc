#include "shv/cp_pack.h"
#include <shv/rpcmsg.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


void rpcmsg_pack_request(
	cp_pack_t pack, const char *path, const char *method, int64_t rid) {
	cp_pack_meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID);
	cp_pack_int(pack, 1);
	cp_pack_int(pack, RPCMSG_TAG_REQUEST_ID);
	cp_pack_int(pack, rid);
	cp_pack_int(pack, RPCMSG_TAG_SHV_PATH);
	cp_pack_str(pack, path);
	cp_pack_int(pack, RPCMSG_TAG_METHOD);
	cp_pack_str(pack, method);
	cp_pack_container_end(pack);

	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCMSG_KEY_PARAMS);
}

void rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *method) {
	cp_pack_meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID);
	cp_pack_int(pack, 1);
	cp_pack_int(pack, RPCMSG_TAG_SHV_PATH);
	cp_pack_str(pack, path);
	cp_pack_int(pack, RPCMSG_TAG_METHOD);
	cp_pack_str(pack, method);
	cp_pack_container_end(pack);

	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCMSG_KEY_PARAMS);
}

static void _pack_response(cp_pack_t pack, struct rpcmsg_meta *meta) {
	cp_pack_meta_begin(pack);
	cp_pack_int(pack, RPCMSG_TAG_META_TYPE_ID);
	cp_pack_int(pack, 1);
	cp_pack_int(pack, RPCMSG_TAG_SHV_PATH);
	cp_pack_str(pack, meta->path);
	cp_pack_int(pack, RPCMSG_TAG_METHOD);
	cp_pack_str(pack, meta->method);
	// TODO copy over other parameters
	cp_pack_container_end(pack);

	cp_pack_imap_begin(pack);
}

void rpcmsg_pack_response(cp_pack_t pack, struct rpcmsg_meta *meta) {
	_pack_response(pack, meta);
	cp_pack_int(pack, RPCMSG_KEY_RESULT);
}

void rpcmsg_pack_error(cp_pack_t pack, struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *msg) {
	_pack_response(pack, meta);
	cp_pack_int(pack, RPCMSG_KEY_ERROR);
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCMSG_E_KEY_CODE);
	cp_pack_int(pack, error);
	cp_pack_int(pack, RPCMSG_E_KEY_MESSAGE);
	cp_pack_str(pack, msg);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
}

void rpcmsg_pack_ferror(cp_pack_t pack, struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	rpcmsg_pack_vferror(pack, meta, error, fmt, args);
	va_end(args);
}

void rpcmsg_pack_vferror(cp_pack_t pack, struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, va_list args) {
	va_list argsdup;
	va_copy(argsdup, args);
	size_t siz = vsnprintf(NULL, 0, fmt, argsdup);
	va_end(argsdup);

	char msg[siz + 1];
	vsnprintf(msg, siz + 1, fmt, args);

	rpcmsg_pack_error(pack, meta, error, msg);
}
