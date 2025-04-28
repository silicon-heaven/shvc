#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <shv/cp.h>
#include "common.h"

#define PUTC(V) \
	do { \
		int __c = (V); \
		if (f && fputc_unlocked(__c, f) != __c) \
			return -1; \
		res++; \
	} while (false)
#define PUTS(V) \
	do { \
		const char *__v = (V); \
		size_t __strlen = strlen(__v); \
		if (f && fwrite_unlocked(__v, 1, __strlen, f) != __strlen) \
			return -1; \
		res += __strlen; \
	} while (false)
#define PRINTF(...) \
	do { \
		size_t __cnt; \
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
		size_t __cnt = FUNC(f, __VA_ARGS__); \
		if (__cnt == -1) \
			return -1; \
		res += __cnt; \
	} while (false)
#define PUTHEX(V) \
	do { \
		int __v = (V) & 0xf; \
		PUTC(__v + (__v >= 10 ? 'A' - 10 : '0')); \
	} while (false)
#define PUTUINT(V) \
	do { \
		uintmax_t __u = (V); \
		if (__u < 10) \
			PUTC((int)__u + '0'); \
		else { \
			char buf[40]; \
			buf[39] = '\0'; \
			int __i; \
			for (__i = 1; __i <= 39; __i++) { \
				int __d = __u % 10; \
				__u /= 10; \
				buf[39 - __i] = __d + '0'; \
				if (__u == 0) \
					break; \
			} \
			PUTS(buf + (39 - __i)); \
		} \
	} while (false)
static_assert(sizeof(uintmax_t) <= 16,
	"The implementation expects uintmax_t to be at most 128bits");
#define PUTINT(V) \
	do { \
		intmax_t __v = (V); \
		if (__v < 0) \
			PUTC('-'); \
		PUTUINT(imaxabs(__v)); \
	} while (false)


static ssize_t cpon_pack_decimal(FILE *f, const struct cpdecimal *dec);
static ssize_t cpon_pack_double(FILE *f, const double val);
static ssize_t cpon_pack_buf(FILE *f, const struct cpitem *item);
static ssize_t ctxpush(
	FILE *f, struct cpon_state *state, enum cpitem_type tp, const char *str);
static enum cpitem_type ctxpop(struct cpon_state *state);


ssize_t cpon_pack(FILE *f, struct cpon_state *state, const struct cpitem *item) {
	ssize_t res = 0;
	if (common_pack(&res, f, item))
		return res;

	if (state->depth <= state->cnt) {
		if (state->depth > 0 && item->type != CPITEM_CONTAINER_END &&
			(item->type != CPITEM_STRING || item->as.String.flags & CPBI_F_FIRST) &&
			(item->type != CPITEM_BLOB || item->as.Blob.flags & CPBI_F_FIRST)) {
			struct cpon_state_ctx *ctx = &state->ctx[state->depth - 1];
			if (!ctx->meta) {
				switch (ctx->tp) {
					case CPITEM_LIST:
						if (!ctx->first)
							PUTC(',');
						break;
					case CPITEM_MAP:
					case CPITEM_IMAP:
					case CPITEM_META:
						if (ctx->even) {
							if (!ctx->first)
								PUTC(',');
						} else
							PUTC(':');
					default:
						break;
				}
				ctx->first = false;
				ctx->even = !ctx->even;
			} else
				ctx->meta = false;
		}
		switch (item->type) {
			case CPITEM_NULL:
				PUTS("null");
				break;
			case CPITEM_BOOL:
				PUTS(item->as.Bool ? "true" : "false");
				break;
			case CPITEM_INT:
				PUTINT(item->as.Int);
				break;
			case CPITEM_UINT:
				PUTUINT(item->as.UInt);
				PUTC('u');
				break;
			case CPITEM_DOUBLE:
				CALL(cpon_pack_double, item->as.Double);
				break;
			case CPITEM_DECIMAL:
				CALL(cpon_pack_decimal, &item->as.Decimal);
				break;
			case CPITEM_BLOB:
				if (item->as.Blob.flags & CPBI_F_FIRST)
					PUTS(item->as.Blob.flags & CPBI_F_HEX ? "x\"" : "b\"");
				if (item->as.Blob.len > 0)
					CALL(cpon_pack_buf, item);
				if (item->as.Blob.flags & CPBI_F_LAST)
					PUTC('\"');
				break;
			case CPITEM_STRING:
				if (item->as.Blob.flags & CPBI_F_FIRST)
					PUTS("\"");
				if (item->as.String.len > 0)
					CALL(cpon_pack_buf, item);
				if (item->as.Blob.flags & CPBI_F_LAST)
					PUTC('\"');
				break;
			case CPITEM_DATETIME: {
				struct tm tm = cpdttotm(item->as.Datetime);
				PRINTF("d\"%d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3d", tm.tm_year + 1900,
					tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					(int)(item->as.Datetime.msecs % 1000));
				if (item->as.Datetime.offutc)
					PRINTF("%s%.2d:%.2d\"", item->as.Datetime.offutc > 0 ? "+" : "",
						(int)(item->as.Datetime.offutc / 60),
						abs(item->as.Datetime.offutc) % 60);
				else
					PUTS("Z\"");
				break;
			}
			case CPITEM_LIST:
				CALL(ctxpush, state, CPITEM_LIST, "[");
				break;
			case CPITEM_MAP:
				CALL(ctxpush, state, CPITEM_MAP, "{");
				break;
			case CPITEM_IMAP:
				CALL(ctxpush, state, CPITEM_IMAP, "i{");
				break;
			case CPITEM_META:
				CALL(ctxpush, state, CPITEM_META, "<");
				break;
			case CPITEM_CONTAINER_END:
				switch (ctxpop(state)) {
					case CPITEM_LIST:
						PUTC(']');
						break;
					case CPITEM_MAP:
					case CPITEM_IMAP:
						PUTC('}');
						break;
					case CPITEM_META:
						PUTC('>');
						if (state->depth > 0)
							state->ctx[state->depth - 1].meta = true;
						break;
					default:
						break;
				}
				break;
			default:
				abort(); /* anything else should be handled in common_pack */
				break;
		}
	} else if (state->depth == state->cnt && state->ctx[state->depth - 1].first) {
		PUTS("...");
		state->ctx[state->depth - 1].first = false;
	}

	return res;
}

static ssize_t cpon_pack_buf(FILE *f, const struct cpitem *item) {
	ssize_t res = 0;

	for (size_t i = 0; i < item->as.Blob.len; i++) {
		uint8_t b = item->rbuf[i];
		if (item->type == CPITEM_BLOB && item->as.Blob.flags & CPBI_F_HEX) {
			PUTHEX(b >> 4);
			PUTHEX(b);
		} else {
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
			if (item->type == CPITEM_BLOB && (b < 32 || b >= 127)) {
				PUTC('\\');
				PUTHEX(b >> 4);
				PUTHEX(b);
			} else
				PUTC(b);
		}
	}

	return res;
}

static ssize_t cpon_pack_decimal(FILE *f, const struct cpdecimal *dec) {
	ssize_t res = 0;

	if (dec->exponent <= 6 && dec->exponent >= -9) {
		/* Pack in X.Y notation */
		bool neg = dec->mantissa < 0;
		uint64_t mantissa = neg ? -dec->mantissa : dec->mantissa;

		/* The 64-bit number can ocuppy at most 20 characters plus zero byte */
		static const size_t strsiz = 21;
		char str[strsiz];
		ssize_t len = snprintf(str, strsiz, "%" PRIu64, mantissa);
		assert(len < strsiz);

		if (neg)
			PUTC('-');
		int64_t i = len - 1;
		for (; i >= 0 && i >= -dec->exponent; i--)
			PUTC(str[len - i - 1]);
		for (int64_t z = dec->exponent; z > 0; z--)
			PUTC('0');
		if (len <= -dec->exponent)
			PUTC('0');
		PUTC('.');
		for (int64_t z = -dec->exponent - len; z > 0; z--)
			PUTC('0');
		for (; i >= 0; i--)
			PUTC(str[len - i - 1]);
	} else {
		/* Pack in XeY notation */
		PUTINT(dec->mantissa);
		PUTC('e');
		PUTINT(dec->exponent);
	}

	return res;
}

static ssize_t cpon_pack_double(FILE *f, double val) {
	ssize_t res = 0;
	if (val == 0) {
		/* The simplest case if val is zero. */
		PUTS("0x0.0p+1");
		return res;
	}

	/* Ensure the correct conversion to hex integer */
	union di {
		uint64_t _int;
		double _double;
	};

	/* Buffer (-)0x1. at the begining and p+ at the end. */
	char buf[20] = {'0', 'x', '1', '.', '0'};

	int e;
	union di di;
	di._double = frexp(val, &e) * 2;
	e--;

	bool leading_zeros = true;
	int idx = 4;
	int last = 4;
	for (int i = 48; i >= 0; i -= 4) {
		char c = ((di._int & ((1LL << 52) - 1)) >> (i)) & 0xf;
		/* Avoid leading zeros */
		if (c == 0 && leading_zeros)
			continue;
		leading_zeros = false;
		/* Save last valid number index. This is to cut zeros at the end. */
		if (c != 0)
			last = idx;
		c += c >= 10 ? ('A' - 10) : '0';
		buf[idx++] = c;
	}
	/* Finish the buffer and write the required format. */
	buf[++last] = 'p';
	if (e > 0)
		buf[++last] = '+';
	buf[++last] = '\0';
	if (val < 0)
		PUTC('-');
	PUTS(buf);
	/* PUTINT automatically inserts '-' if value is negative. */
	PUTINT(e);
	return res;
}

static ssize_t ctxpush(
	FILE *f, struct cpon_state *state, enum cpitem_type tp, const char *str) {
	ssize_t res = 0;
	if (state->depth == state->cnt && state->realloc)
		state->realloc(state);
	if (state->depth < state->cnt) {
		state->ctx[state->depth] = (struct cpon_state_ctx){
			.tp = tp,
			.first = true,
			.even = true,
		};
		PUTS(str);
	} else if (state->depth == state->cnt)
		PUTS("...");
	state->depth++;
	return res;
}

static enum cpitem_type ctxpop(struct cpon_state *state) {
	if (state->depth > 0) {
		state->depth--;
		if (state->depth < state->cnt)
			return state->ctx[state->depth].tp;
	}
	return CPITEM_INVALID;
}
