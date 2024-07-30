#include <stdlib.h>
#include <sys/param.h>
#include <assert.h>
#include <shv/cp_unpack.h>

static void cp_unpack_chainpack_func(void *ptr, struct cpitem *item) {
	struct cp_unpack_chainpack *p = ptr;
	chainpack_unpack(p->f, item);
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

static void cp_unpack_cpon_func(void *ptr, struct cpitem *item) {
	struct cp_unpack_cpon *p = ptr;
	cpon_unpack(p->f, &p->state, item);
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

void cp_unpack_drop1(cp_unpack_t unpack, struct cpitem *item) {
	item->buf = NULL;
	item->bufsiz = SIZE_MAX;
	while ((item->type == CPITEM_STRING || item->type == CPITEM_BLOB) &&
		!(item->as.Blob.flags & CPBI_F_LAST))
		cp_unpack(unpack, item);
	item->bufsiz = 0;
}

void cp_unpack_drop(cp_unpack_t unpack, struct cpitem *item) {
	switch (item->type) {
		case CPITEM_LIST:
		case CPITEM_MAP:
		case CPITEM_IMAP:
		case CPITEM_META:
			cp_unpack_finish(unpack, item, 1);
			break;
		default:
			cp_unpack_drop1(unpack, item);
			break;
	}
}

void cp_unpack_skip(cp_unpack_t unpack, struct cpitem *item) {
	cp_unpack_finish(unpack, item, 0);
}

void cp_unpack_finish(cp_unpack_t unpack, struct cpitem *item, unsigned depth) {
	cp_unpack_drop1(unpack, item);
	item->buf = NULL;
	item->bufsiz = SIZE_MAX;
	for_cp_unpack_item(unpack, item, depth);
	item->bufsiz = 0;
}

ssize_t cp_unpack_strncpy(
	cp_unpack_t unpack, struct cpitem *item, char *dest, size_t n) {
	item->chr = dest;
	item->bufsiz = n;
	cp_unpack(unpack, item);
	if (item->type != CPITEM_STRING) {
		item->bufsiz = 0;
		return -1;
	}
	if (item->as.String.len < n)
		dest[item->as.String.len] = '\0';
	item->bufsiz = 0;
	return item->as.String.len;
}

ssize_t cp_unpack_memcpy(
	cp_unpack_t unpack, struct cpitem *item, uint8_t *dest, size_t siz) {
	item->buf = dest;
	item->bufsiz = siz;
	cp_unpack(unpack, item);
	item->bufsiz = 0;
	if (item->type != CPITEM_BLOB)
		return -1;
	return item->as.Blob.len;
}

static void *cp_unpack_dup(cp_unpack_t unpack, struct cpitem *item,
	enum cpitem_type type, size_t *size, size_t len) {
#define res_realloc(siz) \
	do { \
		uint8_t *tmp_res = realloc(res, siz); \
		if (tmp_res == NULL) { \
			free(res); \
			return NULL; \
		} \
		res = tmp_res; \
	} while (false)

	uint8_t *res = NULL;
	size_t off = 0;
	do {
		item->bufsiz = MAX(off, 4);
		if (len < (item->bufsiz + off))
			item->bufsiz = len - off;
		res_realloc(off + item->bufsiz);
		item->buf = res + off;
		cp_unpack(unpack, item);
		if (item->type != type) {
			free(res);
			item->bufsiz = 0;
			return NULL;
		}
		off += item->as.Blob.len;
	} while (off < len && !(item->as.Blob.flags & CPBI_F_LAST));
	item->bufsiz = 0;
	if (size) {
		*size = off;
		res_realloc(off);
	} else {
		res_realloc(off + 1);
		res[off] = '\0';
	}
	return res;
#undef res_realloc
}

char *cp_unpack_strdup(cp_unpack_t unpack, struct cpitem *item) {
	return cp_unpack_dup(unpack, item, CPITEM_STRING, NULL, SIZE_MAX);
}

char *cp_unpack_strndup(cp_unpack_t unpack, struct cpitem *item, size_t len) {
	return cp_unpack_dup(unpack, item, CPITEM_STRING, NULL, len);
}

void cp_unpack_memdup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **buf, size_t *siz) {
	*buf = cp_unpack_dup(unpack, item, CPITEM_BLOB, siz, SIZE_MAX);
}

void cp_unpack_memndup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **buf, size_t *siz) {
	*buf = cp_unpack_dup(unpack, item, CPITEM_BLOB, siz, *siz);
}

static bool cp_unpack_dupog(cp_unpack_t unpack, struct cpitem *item,
	enum cpitem_type type, size_t len, struct obstack *obstack) {
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
		item->chr = obstack_next_free(obstack) - (zeropad * sizeof *item->chr);
		size_t room = obstack_room(obstack) + zeropad;
		size_t limit = len - obstack_object_size(obstack);
		item->bufsiz = MIN(room, limit);
		cp_unpack(unpack, item);
		if (item->type != type) {
			obstack_free(obstack, obstack_base(obstack));
			item->bufsiz = 0;
			return false;
		}
		if (item->as.Blob.len > 0) {
			obstack_blank_fast(obstack, item->as.Blob.len - zeropad);
			zeropad = 0;
		}
	} while (obstack_object_size(obstack) < len &&
		!(item->as.Blob.flags & CPBI_F_LAST));
	item->bufsiz = 0;
	if (zeropad) {
		if (type != CPITEM_STRING)
			obstack_blank(obstack, -zeropad);
	} else {
		if (type == CPITEM_STRING)
			obstack_1grow(obstack, '\0');
	}
	return true;
}

char *cp_unpack_strdupo(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_strdupog(unpack, item, obstack))
		return obstack_finish(obstack);
	return NULL;
}

bool cp_unpack_strdupog(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	return cp_unpack_dupog(unpack, item, CPITEM_STRING, SIZE_MAX, obstack);
}

char *cp_unpack_strndupo(cp_unpack_t unpack, struct cpitem *item, size_t len,
	struct obstack *obstack) {
	if (cp_unpack_strndupog(unpack, item, len, obstack))
		return obstack_finish(obstack);
	return NULL;
}

bool cp_unpack_strndupog(cp_unpack_t unpack, struct cpitem *item, size_t len,
	struct obstack *obstack) {
	return cp_unpack_dupog(unpack, item, CPITEM_STRING, len, obstack);
}

void cp_unpack_memdupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack) {
	cp_unpack_memdupog(unpack, item, obstack);
	*siz = obstack_object_size(obstack);
	*buf = obstack_finish(obstack);
}

bool cp_unpack_memdupog(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	return cp_unpack_dupog(unpack, item, CPITEM_BLOB, SIZE_MAX, obstack);
}

void cp_unpack_memndupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack) {
	cp_unpack_memndupog(unpack, item, *siz, obstack);
	*siz = obstack_object_size(obstack);
	*buf = obstack_finish(obstack);
}

bool cp_unpack_memndupog(cp_unpack_t unpack, struct cpitem *item, size_t siz,
	struct obstack *obstack) {
	return cp_unpack_dupog(unpack, item, CPITEM_BLOB, siz, obstack);
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
	c->item->bufsiz = 0;
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
