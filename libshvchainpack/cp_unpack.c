#include "shv/cp.h"
#include <shv/cp_unpack.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/param.h>

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

size_t cp_unpack_drop(cp_unpack_t unpack, struct cpitem *item) {
	size_t res = 0;
	item->buf = NULL;
	item->bufsiz = SIZE_MAX;
	while ((item->type == CPITEM_STRING || item->type == CPITEM_BLOB) &&
		!(item->as.Blob.flags & CPBI_F_LAST))
		res += cp_unpack(unpack, item);
	item->bufsiz = 0;
	return res;
}

size_t cp_unpack_skip(cp_unpack_t unpack, struct cpitem *item) {
	return cp_unpack_finish(unpack, item, 0);
}

size_t cp_unpack_finish(cp_unpack_t unpack, struct cpitem *item, unsigned depth) {
	size_t res = 0;
	cp_unpack_drop(unpack, item);
	item->bufsiz = SIZE_MAX;
	do {
		res += cp_unpack(unpack, item);
		switch (item->type) {
			case CPITEM_BLOB:
			case CPITEM_STRING:
				if (item->as.Blob.flags & CPBI_F_FIRST)
					depth++;
				if (item->as.Blob.flags & CPBI_F_LAST)
					depth--;
				break;
			case CPITEM_LIST:
			case CPITEM_MAP:
			case CPITEM_IMAP:
			case CPITEM_META:
				depth++;
				break;
			case CPITEM_CONTAINER_END:
				depth--;
				break;
			case CPITEM_INVALID:
				return false;
			default:
				break;
		}
	} while (depth);
	item->bufsiz = 0;
	return res;
}

static void *cp_unpack_dup(
	cp_unpack_t unpack, struct cpitem *item, size_t *size, size_t len) {
	uint8_t *res = NULL;
	size_t off = 0;
	do {
		item->bufsiz = MAX(off, 4);
		if (len < (item->bufsiz + off))
			item->bufsiz = len - off;
		res = realloc(res, off + item->bufsiz);
		assert(res);
		item->buf = res + off;
		cp_unpack(unpack, item);
		if (item->type != (size ? CPITEM_BLOB : CPITEM_STRING)) {
			free(res);
			return NULL;
		}
		off += item->as.Blob.len;
	} while (off < len && !(item->as.Blob.flags & CPBI_F_LAST));
	if (size) {
		*size = off;
		return realloc(res, off);
	} else {
		res = realloc(res, off + 1);
		res[off] = '\0';
		return res;
	}
}

char *cp_unpack_strdup(cp_unpack_t unpack, struct cpitem *item) {
	return cp_unpack_dup(unpack, item, NULL, SIZE_MAX);
}

char *cp_unpack_strndup(cp_unpack_t unpack, struct cpitem *item, size_t len) {
	return cp_unpack_dup(unpack, item, NULL, len);
}

void cp_unpack_memdup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **buf, size_t *siz) {
	*buf = cp_unpack_dup(unpack, item, siz, SIZE_MAX);
}

void cp_unpack_memndup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **buf, size_t *siz) {
	*buf = cp_unpack_dup(unpack, item, siz, *siz);
}

static void *cp_unpack_dupo(cp_unpack_t unpack, struct cpitem *item,
	size_t *size, size_t len, struct obstack *obstack) {
	int zeropad = 0;
	do {
		/* We are using here fast growing in a reverse way. We first store
		 * data and only after that we grow object. We do this because we
		 * do not know number of needed bytes upfront. This way we do not
		 * have to use temporally buffer and copy data around. We copy it
		 * directly to the obstack's buffer.
		 */
		if (obstack_room(obstack) == 0) {
			obstack_1grow(obstack, '\0'); /* Force allocation of more room */
			zeropad = 1;
		}
		item->chr = obstack_next_free(obstack);
		size_t room = obstack_room(obstack) + zeropad;
		size_t limit = len - obstack_object_size(obstack);
		item->bufsiz = MIN(room, limit);
		cp_unpack(unpack, item);
		if (item->type != (size ? CPITEM_BLOB : CPITEM_STRING)) {
			obstack_free(obstack, obstack_base(obstack));
			item->bufsiz = 0;
			return NULL;
		}
		if (item->as.Blob.len > 0) {
			obstack_blank_fast(obstack, item->as.Blob.len - zeropad);
			zeropad = 0;
		}
	} while (obstack_object_size(obstack) < len &&
		!(item->as.Blob.flags & CPBI_F_LAST));
	if (size)
		*size = obstack_object_size(obstack) - zeropad;
	else if (zeropad == 0)
		obstack_1grow(obstack, '\0');
	return obstack_finish(obstack);
}

char *cp_unpack_strdupo(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	return cp_unpack_dupo(unpack, item, NULL, SIZE_MAX, obstack);
}

char *cp_unpack_strndupo(cp_unpack_t unpack, struct cpitem *item, size_t len,
	struct obstack *obstack) {
	return cp_unpack_dupo(unpack, item, NULL, len, obstack);
}

void cp_unpack_memdupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack) {
	*buf = cp_unpack_dupo(unpack, item, siz, SIZE_MAX, obstack);
}

void cp_unpack_memndupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack) {
	*buf = cp_unpack_dupo(unpack, item, siz, *siz, obstack);
}

int cp_unpack_expect_str(
	cp_unpack_t unpack, struct cpitem *item, const char **strings) {
	// TODO
	return -2;
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
	if (c->item->type != (blob ? CPITEM_BLOB : CPITEM_STRING))
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
	if (item->type != CPITEM_BLOB && item->type != CPITEM_STRING)
		return NULL;

	struct cookie *cookie = malloc(sizeof *cookie);
	*cookie = (struct cookie){
		.unpack = unpack,
		.item = item,
	};
	return fopencookie(
		cookie, "r", item->type == CPITEM_STRING ? func_string : func_blob);
}
