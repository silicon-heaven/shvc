#include "cpon.h"
#include <string.h>
#include <inttypes.h>
#include <assert.h>


static size_t cpon_unpack_buf(FILE *f, struct cpbuf *buf, bool *ok);


size_t cpon_unpack(FILE *f, struct cpitem *item) {
	size_t res = 0;
#define GETC \
	({ \
		int __v = getc(f); \
		if (__v == EOF) { \
			item->type = CP_ITEM_INVALID; \
			return res; \
		} \
		res++; \
		__v; \
	})
#define EXPECT(V) \
	do { \
		char c = GETC; \
		if (c != (V)) { \
			ungetc(c, f); \
			item->type = CP_ITEM_INVALID; \
			return res; \
		} \
	} while (false)
#define EXPECTS(V) \
	do { \
		size_t __expects_len = strlen(V); \
		for (size_t __expect_i = 0; __expect_i < __expects_len; __expect_i++) \
			EXPECT((V)[__expect_i]); \
	} while(false)
#define CALL(FUNC, ...) \
	do { \
		bool ok; \
		res += FUNC(f, __VA_ARGS__, &ok); \
		if (!ok) { \
			item->type = CP_ITEM_INVALID; \
			return res; \
		} \
	} while (false)

	/* Continue reading previous item */
	if ((item->type == CP_ITEM_STRING || item->type == CP_ITEM_BLOB) &&
		!item->as.String.last) {
		CALL(cpon_unpack_buf, &item->as.String);
		return res;
	}

	bool comment = false;
	char c;
	while (true) {
		c = GETC;
		if (comment) {
			if (c == '*') {
				c = GETC;
				comment = c != '/';
			}
			continue;
		}
		if (c == ' ' || c == '\t' || c == ':' || c == ',')
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
			item->type = CP_ITEM_NULL;
			break;
		case 't':
			EXPECTS("rue");
			item->type = CP_ITEM_BOOL;
			item->as.Bool = true;
			break;
		case 'f':
			EXPECTS("alse");
			item->type = CP_ITEM_BOOL;
			item->as.Bool = false;
			break;
		case '-':
		case '0' ... '9':
			// TODO
			break;
		case 'b':
			EXPECT('"');
			item->type = CP_ITEM_BLOB;
			// TODO load some blob data
			break;
		case '"':
			item->type = CP_ITEM_STRING;
			// TODO load some string
			break;
		case '[':
			item->type = CP_ITEM_LIST;
			break;
		case '{':
			item->type = CP_ITEM_MAP;
			break;
		case 'i':
			EXPECT('{');
			item->type = CP_ITEM_IMAP;
			break;
		case '<':
			item->type = CP_ITEM_META;
			break;
		case '>':
		case '}':
		case ']':
			item->type = CP_ITEM_CONTAINER_END;
			break;
		default:
			ungetc(c, f);
			item->type = CP_ITEM_INVALID;
			break;
	}
	return res;
}
