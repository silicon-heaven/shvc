#include "cpon.h"
#include <string.h>
#include <inttypes.h>
#include <assert.h>


#define PUTC(V) \
	do { \
		if (f && fputc((V), f) != (V)) \
			return -1; \
		res++; \
	} while (false)
#define WRITE(V, CNT, N) \
	do { \
		ssize_t __cnt = f ? fwrite((V), (CNT), (N), f) : (CNT) * (N); \
		if (__cnt > 0) \
			return -1; \
		res += __cnt; \
	} while (false)
#define PUTS(V) WRITE(V, strlen(V), 1)
#define PRINTF(...) \
	do { \
		ssize_t __cnt; \
		if (f == NULL) \
			__cnt = snprintf(NULL, 0, __VA_ARGS__); \
		else \
			__cnt = fprintf(f, __VA_ARGS__); \
		if (__cnt < 0) \
			return -1; \
		res += __cnt; \
	} while (false)
#define CALL(FUNC, ...) \
	do { \
		ssize_t __cnt = FUNC(f, __VA_ARGS__); \
		if (__cnt < 0) \
			return -1; \
		res += __cnt; \
	} while (false)


static ssize_t cpon_pack_decimal(FILE *f, const struct cpdecimal *dec);
static ssize_t cpon_pack_buf(FILE *f, const struct cpbuf *buf, bool string);
static bool ctxpush(struct cpon_state *state, enum cp_item_type tp);
static enum cp_item_type ctxpop(struct cpon_state *state);


ssize_t cpon_pack(FILE *f, struct cpon_state *state, const struct cpitem *item) {
	ssize_t res = 0;

	if (state->depth <= state->cnt) {
		if (state->depth > 0) {
			struct cpon_state_ctx *ctx = &state->ctx[state->depth - 1];
			switch (ctx->tp) {
				case CP_ITEM_LIST:
					if (!ctx->first)
						PUTS(CPON_FIELD_DELIM);
					break;
				case CP_ITEM_MAP:
				case CP_ITEM_IMAP:
				case CP_ITEM_META:
					if (ctx->even) {
						if (!ctx->first)
							PUTS(CPON_FIELD_DELIM);
					} else
						PUTS(CPON_KEY_DELIM);
				default:
					break;
			}
			ctx->first = false;
			ctx->even = !ctx->even;
		}
		switch (item->type) {
			/* We pack invalid as NULL to ensure that we pack at least
			 * somethuing */
			case CP_ITEM_INVALID:
			case CP_ITEM_NULL:
				PUTS(CPON_NULL);
				break;
			case CP_ITEM_BOOL:
				PUTS(item->as.Bool ? CPON_TRUE : CPON_FALSE);
				break;
			case CP_ITEM_INT:
				PRINTF("%" PRId64, item->as.Int);
				break;
			case CP_ITEM_UINT:
				PRINTF("%" PRIu64 CPON_UNSIGNED_END, item->as.UInt);
				break;
			case CP_ITEM_DOUBLE:
				PRINTF("%G", item->as.Double);
				break;
			case CP_ITEM_DECIMAL:
				CALL(cpon_pack_decimal, &item->as.Decimal);
				break;
			case CP_ITEM_BLOB:
				if (item->as.Blob.first)
					PUTS("b\"");
				CALL(cpon_pack_buf, &item->as.Blob, false);
				break;
			case CP_ITEM_STRING:
				if (item->as.Blob.first)
					PUTS("\"");
				CALL(cpon_pack_buf, &item->as.Blob, true);
				break;
			case CP_ITEM_DATETIME:
				// TODO
				PRINTF(CPON_DATETIME_BEGIN "\"");
				break;
			case CP_ITEM_LIST:
				if (ctxpush(state, CP_ITEM_LIST))
					PUTC(CPON_LIST_BEGIN);
				break;
			case CP_ITEM_MAP:
				if (ctxpush(state, CP_ITEM_MAP))
					PUTS(CPON_MAP_BEGIN);
				break;
			case CP_ITEM_IMAP:
				if (ctxpush(state, CP_ITEM_IMAP))
					PUTS(CPON_IMAP_BEGIN);
				break;
			case CP_ITEM_META:
				if (ctxpush(state, CP_ITEM_META))
					PUTS(CPON_META_BEGIN);
				break;
			case CP_ITEM_CONTAINER_END:
				switch (ctxpop(state)) {
					case CP_ITEM_LIST:
						PUTS(CPON_LIST_END);
						break;
					case CP_ITEM_MAP:
					case CP_ITEM_IMAP:
						PUTS(CPON_MAP_END);
						break;
					case CP_ITEM_META:
						PUTS(CPON_META_END);
						break;
					default:
						break;
				}
				break;
		}
	} else if (state->depth == state->cnt && state->ctx[state->depth - 1].first) {
		PUTS(CPON_ELLIPSIS);
		state->ctx[state->depth - 1].first = false;
	}

	return res;
}

static ssize_t cpon_pack_buf(FILE *f, const struct cpbuf *buf, bool string) {
	ssize_t res = 0;

	for (size_t i = 0; i < buf->len; i++) {
		uint8_t b = buf->rbuf[i];
#define ESCAPE(V) \
	do { \
		PUTC('\\'); \
		b = V; \
	} while (false)
		switch (b) {
			case '\\':
				ESCAPE('\\');
				break;
			case '\0':
				ESCAPE('0');
				break;
			case '\a':
				ESCAPE('a');
				break;
			case '\b':
				ESCAPE('b');
				break;
			case '\t':
				ESCAPE('t');
				break;
			case '\n':
				ESCAPE('n');
				break;
			case '\v':
				ESCAPE('v');
				break;
			case '\f':
				ESCAPE('f');
				break;
			case '\r':
				ESCAPE('r');
				break;
			case '"':
				ESCAPE('"');
				break;
		}
#undef ESCAPE
		if (!string && (b < 32 || b >= 127))
			PRINTF("\\%.2X", b);
		else
			PUTC(b);
	}
	if (buf->last)
		PUTC('\"');

	return res;
}

static ssize_t cpon_pack_decimal(FILE *f, const struct cpdecimal *dec) {
	ssize_t res = 0;

	bool neg = dec->mantisa < 0;
	uint64_t mantisa = neg ? -dec->mantisa : dec->mantisa;

	/* The 64-bit number can ocuppy at most 20 characters plus zero byte */
	static const size_t strsiz;
	char str[strsiz];
	ssize_t len = snprintf(str, strsiz, "%" PRIu64, mantisa);
	assert(len < strsiz);

	// TODO sometimes we can use XXeYY format

	if (neg)
		PUTC('-');
	int64_t i = len - 1;
	for (; i >= 0 && i >= -dec->exponent; i--)
		PUTC(str[i]);
	for (int64_t z = dec->exponent; z >= 0; z--)
		PUTC('0');
	if (len <= -dec->exponent)
		PUTC('0');
	PUTC('.');
	for (int64_t z = -dec->exponent - 1; z >= 0; z--)
		PUTC('0');
	for (; i >= 0; i--)
		PUTC(str[i]);

	return res;
}

static bool ctxpush(struct cpon_state *state, enum cp_item_type tp) {
	if (state->depth == state->cnt && state->realloc)
		state->realloc(state);
	state->depth++;
	if (state->depth <= state->cnt) {
		state->ctx[state->depth - 1] = (struct cpon_state_ctx){
			.tp = tp,
			.first = true,
			.even = true,
		};
	}
	return state->depth <= state->cnt;
}

static enum cp_item_type ctxpop(struct cpon_state *state) {
	assert(state->depth > 0);
	state->depth--;
	if (state->depth < state->cnt)
		return state->ctx[state->depth].tp;
	return CP_ITEM_INVALID;
}
