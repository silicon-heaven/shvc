#include <shv/cp.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "common.h"


#define GETC \
	({ \
		int __v = getc(f); \
		res++; \
		__v; \
	})
#define UNGETC(V) \
	do { \
		ungetc((V), f); \
		res--; \
	} while (false)
#define SCANF(FMT, ...) \
	({ \
		size_t __off = ftell(f); \
		ssize_t __res = fscanf(f, FMT, __VA_ARGS__); \
		res += ftell(f) - __off; \
		__res; \
	})


static size_t cpon_unpack_buf(FILE *f, struct cpitem *item, enum cperror *err);


// TODO use state
size_t cpon_unpack(FILE *f, struct cpon_state *state, struct cpitem *item) {
	size_t res = 0;
	if (common_unpack(&res, f, item))
		return res;
#define ERR_IO \
	do { \
		item->type = CPITEM_INVALID; \
		item->as.Error = feof(f) ? CPERR_EOF : CPERR_IO; \
		return res; \
	} while (false)
#define ERR_INVALID \
	do { \
		item->type = CPITEM_INVALID; \
		item->as.Error = CPERR_INVALID; \
		return res; \
	} while (false)
#define EXPECT(V) \
	do { \
		char c = GETC; \
		if (c == EOF) \
			ERR_IO; \
		if (c != (V)) { \
			UNGETC(V); \
			ERR_INVALID; \
		} \
	} while (false)
#define EXPECTS(V) \
	do { \
		size_t __expects_len = strlen(V); \
		for (size_t __expect_i = 0; __expect_i < __expects_len; __expect_i++) \
			EXPECT((V)[__expect_i]); \
	} while (false)
#define CALL(FUNC, ...) \
	do { \
		enum cperror err = CPERR_NONE; \
		res += FUNC(f, __VA_ARGS__, &err); \
		if (err == CPERR_EOF) \
			ERR_IO; \
		else if (err == CPERR_INVALID) \
			ERR_INVALID; \
	} while (false)

	/* Continue reading previous item */
	if ((item->type == CPITEM_BLOB || item->type == CPITEM_STRING) &&
		(item->as.Blob.eoff != 0 || !(item->as.Blob.flags & CPBI_F_LAST))) {
		item->as.Blob.flags &= ~CPBI_F_FIRST;
		CALL(cpon_unpack_buf, item);
		return res;
	}

	item->type = CPITEM_INVALID;
	bool comment = false;
	char c;
	while (true) {
		c = GETC;
		if (c == EOF)
			ERR_IO;
		if (comment) {
			if (c == '*') {
				c = GETC;
				if (c == EOF)
					ERR_IO;
				comment = c != '/';
			}
			continue;
		}
		// TODO : and , should be accepted only in places we know they should be
		// thanks to the cpon status.
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == ',')
			continue;
		if (c == '/') {
			EXPECT('*');
			comment = true;
			continue;
		}
		break;
	}

	switch (c) {
		case 'n':
			EXPECTS("ull");
			item->type = CPITEM_NULL;
			break;
		case 't':
			EXPECTS("rue");
			item->type = CPITEM_BOOL;
			item->as.Bool = true;
			break;
		case 'f':
			EXPECTS("alse");
			item->type = CPITEM_BOOL;
			item->as.Bool = false;
			break;
		case '-':
		case '0' ... '9':
			UNGETC(c);
			if (SCANF("%lli", &item->as.Int) != 1)
				ERR_IO;
			item->type = CPITEM_INT;
			bool has_sign = c == '-';
			c = GETC;
			if (!has_sign && c == 'u') {
				item->type = CPITEM_UINT;
			} else if (c == 'e') {
				item->type = CPITEM_DECIMAL;
				item->as.Decimal.mantisa = item->as.Int;
				item->as.Decimal.exponent = 0;
				SCANF("%i", &item->as.Decimal.exponent);
			} else if (c == '.') {
				item->type = CPITEM_DECIMAL;
				item->as.Decimal.mantisa = item->as.Int;
				unsigned long long dec;
				ssize_t rres = res;
				if (SCANF("%llu", &dec) == 1) {
					item->as.Decimal.exponent = rres - res;
					int64_t mult = 1;
					for (int i = 0; i < -item->as.Decimal.exponent; i++)
						mult *= 10;
					// TODO this is not ideal because we might hit limit. We
					// should add only up to the dec being zero and keep the
					// rest of the exponent as it is.
					item->as.Decimal.mantisa =
						((item->as.Decimal.mantisa < 0 ? -1 : 1) * dec) +
						(item->as.Decimal.mantisa * mult);
				}
				if (item->as.Decimal.mantisa == 0)
					break;
				while (item->as.Decimal.mantisa % 10 == 0) {
					item->as.Decimal.mantisa /= 10;
					item->as.Decimal.exponent++;
				}
			} else
				UNGETC(c);
			break;
		case 'x':
		case 'b':
			EXPECT('"');
			item->type = CPITEM_BLOB;
			item->as.String.flags = CPBI_F_FIRST | CPBI_F_STREAM;
			if (c == 'x')
				item->as.String.flags |= CPBI_F_HEX;
			item->as.String.eoff = 0;
			CALL(cpon_unpack_buf, item);
			break;
		case 'd':
			struct tm tm = (struct tm){};
			unsigned msecs;
			int sres = SCANF("\"%u-%u-%uT%u:%u:%u.%u", &tm.tm_year, &tm.tm_mon,
				&tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &msecs);
			if (sres == -1)
				ERR_IO;
			if (sres != 7)
				ERR_INVALID;
			tm.tm_year -= 1900;
			tm.tm_mon -= 1;
			c = GETC;
			if (c == '+' || c == '-') {
				unsigned h, m;
				SCANF("%u:%u", &h, &m);
				tm.tm_gmtoff = ((h * 60) + m) * 60;
				tm.tm_gmtoff *= c == '-' ? -1 : 1;
			} else if (c != 'Z') {
				UNGETC(c);
				ERR_INVALID;
			}
			EXPECT('"');
			item->type = CPITEM_DATETIME;
			item->as.Datetime = cptmtodt(tm);
			item->as.Datetime.msecs += msecs;
			break;
		case '"':
			item->type = CPITEM_STRING;
			item->as.String.flags = CPBI_F_FIRST | CPBI_F_STREAM;
			item->as.String.eoff = 0;
			CALL(cpon_unpack_buf, item);
			break;
		case '[':
			item->type = CPITEM_LIST;
			break;
		case '{':
			item->type = CPITEM_MAP;
			break;
		case 'i':
			EXPECT('{');
			item->type = CPITEM_IMAP;
			break;
		case '<':
			item->type = CPITEM_META;
			break;
		case '>':
		case '}':
		case ']':
			item->type = CPITEM_CONTAINER_END;
			break;
		default:
			UNGETC(c);
			break;
	}
	return res;
}

static size_t cpon_unpack_buf(FILE *f, struct cpitem *item, enum cperror *err) {
	if (item->bufsiz == 0) {
		item->as.Blob.len = 0;
		return 0; /* Nowhere to place data so just inform user about type */
	}

	size_t i = 0;
	size_t res = 0;
	bool escape = false;
	while (i < item->bufsiz) {
		int c = getc(f);
		if (c == EOF) {
			*err = CPERR_EOF;
			return i;
		}
		res++;
		if (item->type == CPITEM_BLOB && item->as.Blob.flags & CPBI_F_HEX) {
			if (c == '"') {
				item->as.Blob.flags |= CPBI_F_LAST;
				break;
			}
			UNGETC(c);
			uint8_t byte;
			int sres = SCANF("%2" SCNx8, &byte);
			if (sres != 1) {
				*err = sres == -1 ? CPERR_EOF : CPERR_INVALID;
				return i;
			}
			if (item->buf)
				item->buf[i++] = byte;
		} else if (escape) {
			switch (c) {
				case 'a':
					c = '\a';
					break;
				case 'b':
					c = '\b';
					break;
				case 't':
					c = '\t';
					break;
				case 'n':
					c = '\n';
					break;
				case 'v':
					c = '\v';
					break;
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
			}
			if (item->type == CPITEM_BLOB) {
				UNGETC(c);
				SCANF("%X", (unsigned *)&c);
			}
			if (item->buf)
				item->buf[i++] = c;
			escape = false;
		} else {
			if (c == '\\') {
				escape = true;
			} else if (c == '"') {
				item->as.Blob.flags |= CPBI_F_LAST;
				break;
			} else if (item->buf)
				item->buf[i++] = c;
		}
	}
	item->as.Blob.len = i;
	return res;
}
