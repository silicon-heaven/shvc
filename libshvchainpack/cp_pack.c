#include <shv/cp_pack.h>
#include <stdlib.h>

static size_t cp_pack_chainpack_func(void *ptr, const struct cpitem *item) {
	struct cp_pack_chainpack *p = ptr;
	return chainpack_pack(p->f, item);
}

cp_pack_t cp_pack_chainpack_init(struct cp_pack_chainpack *pack, FILE *f) {
	*pack = (struct cp_pack_chainpack){
		.func = cp_pack_chainpack_func,
		.f = f,
	};
	return &pack->func;
}

static void cpon_state_realloc(struct cpon_state *state) {
	state->cnt = state->cnt ? state->cnt * 2 : 1;
	state->ctx = realloc(state->ctx, state->cnt * sizeof *state->ctx);
}

static size_t cp_pack_cpon_func(void *ptr, const struct cpitem *item) {
	struct cp_pack_cpon *p = ptr;
	return cpon_pack(p->f, &p->state, item);
}

cp_pack_t cp_pack_cpon_init(struct cp_pack_cpon *pack, FILE *f, const char *indent) {
	*pack = (struct cp_pack_cpon){
		.func = cp_pack_cpon_func,
		.f = f,
		.state =
			(struct cpon_state){
				.indent = indent,
				.depth = 0,
				.cnt = 0,
				.realloc = cpon_state_realloc,
			},
	};
	return &pack->func;
}


static ssize_t _write(void *cookie, const char *buf, size_t size, bool blob) {
	cp_pack_t pack = cookie;
	struct cpitem item;
	item.type = blob ? CPITEM_BLOB : CPITEM_STRING;
	item.rchr = buf;
	item.as.Blob = (struct cpbufinfo){
		.len = size,
		.flags = CPBI_F_STREAM,
	};
	ssize_t res = cp_pack(pack, &item);
	return res >= 0 ? size : 0;
}

static int _close(void *cookie, bool blob) {
	cp_pack_t pack = cookie;
	struct cpitem item;
	item.type = blob ? CPITEM_BLOB : CPITEM_STRING;
	item.as.Blob = (struct cpbufinfo){
		.len = 0,
		.flags = CPBI_F_STREAM | CPBI_F_LAST,
	};
	ssize_t res = cp_pack(pack, &item);
	return res >= 0 ? 0 : EOF;
}

static ssize_t write_blob(void *cookie, const char *buf, size_t size) {
	return _write(cookie, buf, size, true);
}

static int close_blob(void *cookie) {
	return _close(cookie, true);
}

static const cookie_io_functions_t func_blob = {
	.write = write_blob,
	.close = close_blob,
};


static ssize_t write_string(void *cookie, const char *buf, size_t size) {
	return _write(cookie, buf, size, false);
}

static int close_string(void *cookie) {
	return _close(cookie, false);
}

static const cookie_io_functions_t func_string = {
	.write = write_string,
	.close = close_string,
};

FILE *cp_pack_fopen(cp_pack_t pack, bool str) {
	struct cpitem item;
	item.type = str ? CPITEM_STRING : CPITEM_BLOB;
	item.as.Blob = (struct cpbufinfo){
		.len = 0,
		.flags = CPBI_F_FIRST | CPBI_F_STREAM,
	};
	if (cp_pack(pack, &item) == 0)
		return NULL;
	return fopencookie(pack, "a", str ? func_string : func_blob);
}
