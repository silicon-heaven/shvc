#include "shv/cp.h"
#include <shv/cp_unpack.h>
#include <stdlib.h>
#include <assert.h>

static size_t cp_unpack_chainpack_func(void *ptr, struct cpitem *item) {
	struct cp_unpack_chainpack *p = ptr;
	return chainpack_unpack(p->f, item);
}

cp_unpack_t cp_unpack_chainpack_init(struct cp_unpack_chainpack *unpack, FILE *f) {
	*unpack = (struct cp_unpack_chainpack){
		.func = cp_unpack_chainpack_func,
		.f = f,
	};
	return &unpack->func;
}

static void cpon_state_realloc(struct cpon_state *state) {
	state->cnt = state->cnt ? state->cnt * 2 : 1;
	state->ctx = realloc(state->ctx, state->cnt * sizeof *state->ctx);
}

static size_t cp_unpack_cpon_func(void *ptr, struct cpitem *item) {
	struct cp_unpack_cpon *p = ptr;
	return cpon_unpack(p->f, &p->state, item);
}

cp_unpack_t cp_unpack_cpon_init(struct cp_unpack_cpon *unpack, FILE *f) {
	*unpack = (struct cp_unpack_cpon){
		.func = cp_unpack_cpon_func,
		.f = f,
		.state =
			(struct cpon_state){
				.depth = 0,
				.cnt = 0,
				.realloc = cpon_state_realloc,
			},
	};
	return &unpack->func;
}

static size_t _cp_unpack_skip(
	cp_unpack_t unpack, struct cpitem *item, unsigned depth) {
	size_t res = 0;
	item->buf = NULL;
	item->bufsiz = SIZE_MAX;
	if ((item->type == CP_ITEM_STRING || item->type == CP_ITEM_BLOB) &&
		!(item->as.Blob.flags & CPBI_F_LAST)) {
		depth++;
	}
	do {
		res += cp_unpack(unpack, item);
		switch (item->type) {
			case CP_ITEM_BLOB:
			case CP_ITEM_STRING:
				if (item->as.Blob.flags & CPBI_F_FIRST)
					depth++;
				if (item->as.Blob.flags & CPBI_F_LAST)
					depth--;
				break;
			case CP_ITEM_LIST:
			case CP_ITEM_MAP:
			case CP_ITEM_IMAP:
			case CP_ITEM_META:
				depth++;
				break;
			case CP_ITEM_CONTAINER_END:
				depth--;
				break;
			case CP_ITEM_INVALID:
				return false;
			default:
				break;
		}
	} while (depth);
	return res;
}

size_t cp_unpack_skip(cp_unpack_t unpack, struct cpitem *item) {
	return _cp_unpack_skip(unpack, item, 0);
}

size_t cp_unpack_finish(cp_unpack_t unpack, struct cpitem *item) {
	return _cp_unpack_skip(unpack, item, 1);
}

static void *cp_unpack_dup(cp_unpack_t unpack, struct cpitem *item, size_t *size) {
	item->bufsiz = 2;
	uint8_t *res = malloc(item->bufsiz);
	size_t off = 0;
	item->buf = res;
	while (true) {
		cp_unpack(unpack, item);
		if (item->type != (size ? CP_ITEM_BLOB : CP_ITEM_STRING)) {
			free(res);
			return NULL;
		}
		if (item->as.Blob.flags & CPBI_F_LAST) {
			off += item->as.Blob.len;
			if (size) {
				*size = off;
				return realloc(res, off);
			} else {
				res = realloc(res, off + 1);
				res[off] = '\0';
				return res;
			}
		}
		/* We expect here that all available bytes are used */
		assert(item->as.Blob.len == item->bufsiz);
		off += item->bufsiz;
		item->bufsiz = off;
		res = realloc(res, off * 2);
		item->buf = res + off;
	}
	return res;
}

char *cp_unpack_strdup(cp_unpack_t unpack, struct cpitem *item) {
	return cp_unpack_dup(unpack, item, NULL);
}

void cp_unpack_memdup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **buf, size_t *siz) {
	*buf = cp_unpack_dup(unpack, item, siz);
}


struct cookie {
	cp_unpack_t unpack;
	struct cpitem *item;
};

static ssize_t _read(void *cookie, char *buf, size_t size, bool blob) {
	struct cookie *c = cookie;
	if (c->item->as.String.flags & CPBI_F_LAST)
		return 0;
	c->item->chr = buf;
	c->item->bufsiz = size;
	cp_unpack(c->unpack, c->item);
	if (c->item->type != (blob ? CP_ITEM_BLOB : CP_ITEM_STRING))
		return -1;
	return c->item->as.String.len;
}

static int _close(void *cookie, bool blob) {
	struct cookie *c = cookie;
	free(c);
	return 0;
}

static ssize_t read_blob(void *cookie, char *buf, size_t size) {
	return _read(cookie, buf, size, true);
}

static int close_blob(void *cookie) {
	return _close(cookie, true);
}

static const cookie_io_functions_t func_blob = {
	.read = read_blob,
	.close = close_blob,
};


static ssize_t read_string(void *cookie, char *buf, size_t size) {
	return _read(cookie, buf, size, false);
}

static int close_string(void *cookie) {
	return _close(cookie, false);
}

static const cookie_io_functions_t func_string = {
	.read = read_string,
	.close = close_string,
};

FILE *cp_unpack_fopen(cp_unpack_t unpack, struct cpitem *item) {
	item->bufsiz = 0;
	cp_unpack(unpack, item);
	if (item->type != CP_ITEM_BLOB && item->type != CP_ITEM_STRING)
		return NULL;

	struct cookie *cookie = malloc(sizeof *cookie);
	*cookie = (struct cookie){
		.unpack = unpack,
		.item = item,
	};
	return fopencookie(
		cookie, "r", item->type == CP_ITEM_STRING ? func_string : func_blob);
}
