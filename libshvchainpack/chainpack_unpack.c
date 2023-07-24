#include "chainpack.h"
#include "shv/cp.h"


static size_t chainpack_unpack_int(FILE *f, int64_t *v, bool *ok);
static size_t chainpack_unpack_buf(FILE *f, struct cpbuf *buf, bool *ok);


size_t chainpack_unpack(FILE *f, struct cpitem *item) {
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
#define READ(PTR, CNT, N) \
	do { \
		ssize_t __v = fread((PTR), (CNT), (N), f); \
		if (__v != (CNT) * (N)) { \
			item->type = CP_ITEM_INVALID; \
			if (__v > 0) \
				res += __v; \
			return res; \
		} \
		res += __v; \
	} while (false)
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
		/* There is no difference between string and blob in data type and thus
		 * we can write this code to serve them both.
		 */
		CALL(chainpack_unpack_buf, &item->as.String);
		return res;
	}

	uint8_t scheme = GETC;
	if (scheme < 0x80) {
		if (scheme & 0x40) {
			item->type = CP_ITEM_INT;
			item->as.Int = scheme & 0x3f;
		} else {
			item->type = CP_ITEM_UINT;
			item->as.UInt = scheme & 0x3f;
		}
	} else {
		switch (scheme) {
			case CPS_Null:
				item->type = CP_ITEM_NULL;
				break;
			case CPS_TRUE:
				item->type = CP_ITEM_BOOL;
				item->as.Bool = true;
				break;
			case CPS_FALSE:
				item->type = CP_ITEM_BOOL;
				item->as.Bool = false;
				break;
			case CPS_Int:
				item->type = CP_ITEM_INT;
				CALL(chainpack_unpack_int, &item->as.Int);
				break;
			case CPS_UInt:
				item->type = CP_ITEM_UINT;
				CALL(_chainpack_unpack_uint, &item->as.UInt);
				break;
			case CPS_Double:
				item->type = CP_ITEM_DOUBLE;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
				uint8_t *b = (uint8_t *)&(item->as.Double);
				for (ssize_t i = sizeof(double) - 1; i >= 0; i--)
					b[i] = GETC;
#else
				READ(&item->as.Double, sizeof(double), 1);
#endif
				break;
			case CPS_Decimal:
				item->type = CP_ITEM_DECIMAL;
				CALL(chainpack_unpack_int, &item->as.Decimal.mantisa);
				int64_t exp;
				CALL(chainpack_unpack_int, &exp);
				item->as.Decimal.exponent = exp;
				break;
			case CPS_Blob:
				item->type = CP_ITEM_BLOB;
				item->as.String.first = true;
				item->as.String.last = false;
				CALL(_chainpack_unpack_uint, (uint64_t *)&item->as.String.eoff);
				CALL(chainpack_unpack_buf, &item->as.Blob);
				break;
			case CPS_String:
				item->type = CP_ITEM_STRING;
				item->as.String.first = true;
				item->as.String.last = false;
				CALL(_chainpack_unpack_uint, (uint64_t *)&item->as.String.eoff);
				CALL(chainpack_unpack_buf, &item->as.String);
				break;
			case CPS_CString:
				item->type = CP_ITEM_STRING;
				item->as.String.first = true;
				item->as.String.last = false;
				item->as.String.eoff = -1;
				CALL(chainpack_unpack_buf, &item->as.String);
				break;
			case CPS_DateTime:
				item->type = CP_ITEM_DATETIME;
				int64_t d;
				CALL(chainpack_unpack_int, &d);
				int32_t offset = 0;
				bool has_tz_offset = d & 1;
				bool has_not_msec = d & 2;
				d >>= 2;
				if (has_tz_offset) {
					offset = d & 0x7F;
					offset = (int8_t)(offset << 1);
					offset >>= 1; /* sign extension */
					d >>= 7;
				}
				if (has_not_msec)
					d *= 1000;
				d += chainpack_epoch_msec;
				item->as.Datetime =
					(struct cpdatetime){.msecs = d, .offutc = offset * 15};
				break;
			case CPS_MetaMap:
				item->type = CP_ITEM_META;
				break;
			case CPS_Map:
				item->type = CP_ITEM_MAP;
				break;
			case CPS_IMap:
				item->type = CP_ITEM_IMAP;
				break;
			case CPS_List:
				item->type = CP_ITEM_LIST;
				break;
			case CPS_TERM:
				item->type = CP_ITEM_CONTAINER_END;
				break;
			default:
				ungetc(scheme, f);
				item->type = CP_ITEM_INVALID;
		}
	}

	return res;
#undef GETC
#undef READ
#undef CALL
}

size_t _chainpack_unpack_uint(FILE *f, uint64_t *v, bool *ok) {
	ssize_t res = 0;

	int head = getc(f);
	if (head == EOF) {
		if (ok)
			*ok = false;
		return res;
	}
	res++;

	*v = 0;
	unsigned getcnt;
	if (!(head & 0x80)) {
		getcnt = 0;
		*v = head & 0x7f;
	} else if (!(head & 0x40)) {
		getcnt = 1;
		*v = head & 0x3f;
	} else if (!(head & 0x20)) {
		getcnt = 2;
		*v = head & 0x1f;
	} else if (!(head & 0x10)) {
		getcnt = 3;
		*v = head & 0x0f;
	} else
		getcnt = (head & 0xf) + 4;

	for (unsigned i = 0; i < getcnt; ++i) {
		int r = getc(f);
		if (r == EOF) {
			if (ok)
				*ok = false;
			return res;
		}
		res++;
		*v = (*v << 8) + r;
	};
	if (ok)
		*ok = true;
	return res;
}

static size_t chainpack_unpack_int(FILE *f, int64_t *v, bool *ok) {
	bool ourok;
	if (ok == NULL)
		ok = &ourok;
	size_t res = _chainpack_unpack_uint(f, (uint64_t *)v, ok);

	if (*ok) {
		/* This is kind of magic that requires some explanation.
		 * We need to calculate where is sign bit. It is always the most
		 * significant bit in the number but the location depends on number of
		 * bytes read. With every byte read there was one most significant bit
		 * used to signal this. That applies for four initial bits.
		 */
		uint64_t sign_mask;
		if (res <= 5)
			sign_mask = 1 << ((8 * res) - res - 1);
		else
			sign_mask = 1 << ((8 * (res - 1)) - 1);
		if (*v & sign_mask) {
			*v &= ~sign_mask;
			*v = -*v;
		}
	}

	return res;
}


static size_t chainpack_unpack_buf(FILE *f, struct cpbuf *buf, bool *ok) {
	if (buf->buf == NULL)
		return 0; /* Nowhere to place data so just inform user about type */

	if (ok)
		*ok = true;

	if (buf->eoff == -1) {
		/* Handle C string */
		size_t i = 0;
		size_t res = 0;
		bool escape = false;
		while (i < buf->siz) {
			int c = getc(f);
			if (c == EOF) {
				if (ok)
					*ok = false;
				return i;
			}
			res++;
			if (escape) {
				if (c == '0')
					c = '\0';
				buf->buf[i++] = c;
				escape = false;
			} else {
				if (c == '\\')
					escape = true;
				else if (c == '\0') {
					buf->last = true;
					break;
				} else
					buf->buf[i++] = c;
			}
		}
		buf->len = i;
		return res;
	}

	size_t toread = buf->eoff > buf->siz ? buf->siz : buf->eoff;
	ssize_t res = fread(buf->buf, toread, 1, f);
	if (res < 0) {
		if (ok)
			*ok = false;
		return 0;
	}
	buf->len = res;
	buf->eoff -= res;
	return res;
}
