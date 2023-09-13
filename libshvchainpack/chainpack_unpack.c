#include <shv/cp.h>
#include <shv/chainpack.h>
#include <sys/param.h>
#include "common.h"


static size_t chainpack_unpack_uint(
	FILE *f, unsigned long long *v, enum cperror *err);
static size_t chainpack_unpack_int(FILE *f, long long *v, enum cperror *err);
static size_t chainpack_unpack_buf(FILE *f, struct cpitem *item, enum cperror *err);


size_t chainpack_unpack(FILE *f, struct cpitem *item) {
	size_t res = 0;
	if (common_unpack(&res, f, item))
		return res;
#define GETC \
	({ \
		int __v = getc(f); \
		if (__v == EOF) { \
			item->type = CPITEM_INVALID; \
			item->as.Error = feof(f) ? CPERR_EOF : CPERR_IO; \
			return res; \
		} \
		res++; \
		__v; \
	})
#define READ(PTR, SIZ) \
	do { \
		size_t __siz = SIZ; \
		ssize_t __v = fread((PTR), __siz, 1, f); \
		if (__v != 1) { \
			item->type = CPITEM_INVALID; \
			item->as.Error = feof(f) ? CPERR_EOF : CPERR_IO; \
			return res; \
		} \
		res += __siz; \
	} while (false)
#define CALL(FUNC, ...) \
	do { \
		enum cperror err = CPERR_NONE; \
		res += FUNC(f, __VA_ARGS__, &err); \
		if (err != CPERR_NONE) { \
			item->type = CPITEM_INVALID; \
			if (err == CPERR_EOF && !feof(f)) \
				item->as.Error = CPERR_IO; \
			else \
				item->as.Error = err; \
			return res; \
		} \
	} while (false)

	/* Continue reading previous item */
	if ((item->type == CPITEM_BLOB || item->type == CPITEM_STRING) &&
		(item->as.Blob.eoff != 0 || !(item->as.Blob.flags & CPBI_F_LAST))) {
		/* There is no difference between string and blob in data type and thus
		 * we can write this code to serve them both.
		 */
		item->as.Blob.flags &= ~CPBI_F_FIRST;
		CALL(chainpack_unpack_buf, item);
		return res;
	}

	uint8_t scheme = GETC;
	if (scheme < CPS_Null) {
		if (chainpack_scheme_signed(scheme)) {
			item->type = CPITEM_INT;
			item->as.Int = chainpack_scheme_uint(scheme);
		} else {
			item->type = CPITEM_UINT;
			item->as.UInt = chainpack_scheme_uint(scheme);
		}
	} else {
		unsigned long long ull;
		switch (scheme) {
			case CPS_Null:
				item->type = CPITEM_NULL;
				break;
			case CPS_TRUE:
				item->type = CPITEM_BOOL;
				item->as.Bool = true;
				break;
			case CPS_FALSE:
				item->type = CPITEM_BOOL;
				item->as.Bool = false;
				break;
			case CPS_Int:
				item->type = CPITEM_INT;
				CALL(chainpack_unpack_int, &item->as.Int);
				break;
			case CPS_UInt:
				item->type = CPITEM_UINT;
				CALL(chainpack_unpack_uint, &item->as.UInt);
				break;
			case CPS_Double:
				item->type = CPITEM_DOUBLE;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
				uint8_t *b = (uint8_t *)&(item->as.Double);
				for (ssize_t i = sizeof(double) - 1; i >= 0; i--)
					b[i] = GETC;
#else
				READ(&item->as.Double, sizeof(double));
#endif
				break;
			case CPS_Decimal:
				item->type = CPITEM_DECIMAL;
				CALL(chainpack_unpack_int, &item->as.Decimal.mantisa);
				long long exp;
				CALL(chainpack_unpack_int, &exp);
				item->as.Decimal.exponent = exp;
				break;
			case CPS_Blob:
			case CPS_BlobChain:
				item->type = CPITEM_BLOB;
				CALL(chainpack_unpack_uint, &ull);
				// TODO check that we do not crop the value
				item->as.String.eoff = ull;
				item->as.Blob.flags = CPBI_F_FIRST;
				if (scheme == CPS_BlobChain)
					item->as.Blob.flags |= CPBI_F_STREAM;
				CALL(chainpack_unpack_buf, item);
				break;
			case CPS_String:
			case CPS_CString:
				item->type = CPITEM_STRING;
				item->as.String.flags = CPBI_F_FIRST;
				if (scheme == CPS_CString) {
					item->as.String.eoff = 0;
					item->as.String.flags |= CPBI_F_STREAM;
				} else {
					CALL(chainpack_unpack_uint, &ull);
					// TODO check that we do not crop the value
					item->as.String.eoff = ull;
				}
				CALL(chainpack_unpack_buf, item);
				break;
			case CPS_DateTime:
				item->type = CPITEM_DATETIME;
				long long d;
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
				d += CHAINPACK_EPOCH_MSEC;
				item->as.Datetime =
					(struct cpdatetime){.msecs = d, .offutc = offset * 15};
				break;
			case CPS_MetaMap:
				item->type = CPITEM_META;
				break;
			case CPS_Map:
				item->type = CPITEM_MAP;
				break;
			case CPS_IMap:
				item->type = CPITEM_IMAP;
				break;
			case CPS_List:
				item->type = CPITEM_LIST;
				break;
			case CPS_TERM:
				item->type = CPITEM_CONTAINER_END;
				break;
			default:
				ungetc(scheme, f);
				item->type = CPITEM_INVALID;
		}
	}

	return res;
#undef GETC
#undef READ
#undef CALL
}

size_t chainpack_unpack_uint(FILE *f, unsigned long long *v, enum cperror *err) {
	ssize_t res = 0;

	int head = getc(f);
	if (head == EOF) {
		*err = CPERR_EOF;
		return res;
	}
	res++;

	unsigned bytes = chainpack_int_bytes(head);

	*v = chainpack_uint_value1(head, bytes);
	for (unsigned i = 1; i < bytes; i++) {
		int r = getc(f);
		if (r == EOF) {
			*err = CPERR_EOF;
			return res;
		}
		res++;
		*v = (*v << 8) | r;
	}

	return res;
}

static size_t chainpack_unpack_int(FILE *f, long long *v, enum cperror *err) {
	size_t res = chainpack_unpack_uint(f, (unsigned long long *)v, err);

	if (*err == CPERR_NONE) {
		/* This is kind of magic that requires some explanation.
		 * We need to calculate where is sign bit. It is always the most
		 * significant bit in the number but the location depends on number of
		 * bytes read. With every byte read there was one most significant bit
		 * used to signal this. That applies for four initial bits.
		 */
		uint64_t sign_mask;
		if (res <= 4)
			sign_mask = 1L << ((8 * res) - res - 1);
		else
			sign_mask = 1L << ((8 * (res - 1)) - 1);
		if (*v & sign_mask) {
			*v &= ~sign_mask;
			*v = -*v;
		}
	}

	return res;
}


static size_t chainpack_unpack_buf(FILE *f, struct cpitem *item, enum cperror *err) {
	if (item->bufsiz == 0) {
		item->as.Blob.len = 0;
		return 0; /* Nowhere to place data so just inform user about type */
	}

	if (item->as.Blob.flags & CPBI_F_STREAM && item->type == CPITEM_STRING) {
		/* Handle C string */
		size_t i;
		for (i = 0; i < item->bufsiz; i++) {
			int c = getc(f);
			if (c == EOF) {
				*err = CPERR_EOF;
				return i;
			}
			if (c == '\0') {
				item->as.Blob.flags |= CPBI_F_LAST;
				item->as.Blob.len = i;
				return i + 1;
			}
			if (item->buf)
				item->buf[i] = c;
		}
		item->as.Blob.len = i;
		return i;
	}

	/* Handle String, Blob and BlobChain */
	size_t res = 0;
	item->as.Blob.len = 0;
	while (item->as.Blob.len < item->bufsiz) {
		size_t toread = MIN(item->as.Blob.eoff, item->bufsiz - item->as.Blob.len);
		if (item->buf) {
			if (toread > 0 &&
				fread(item->buf + item->as.Blob.len, toread, 1, f) != 1) {
				*err = CPERR_EOF;
				break;
			}
		} else {
			for (size_t siz = toread; siz > 0;) {
				size_t bufsiz = MIN(siz, BUFSIZ);
				uint8_t buf[bufsiz];
				if (fread(buf, bufsiz, 1, f) != 1) {
					*err = CPERR_EOF;
					break;
				}
				siz -= bufsiz;
			}
		}
		res += toread;
		item->as.Blob.len = toread;
		item->as.Blob.eoff -= toread;
		if (item->as.Blob.flags & CPBI_F_STREAM && item->as.Blob.eoff == 0) {
			unsigned long long ull;
			res += chainpack_unpack_uint(f, &ull, err);
			if (*err != CPERR_NONE)
				break;
			// TODO check that we do not crop the value
			item->as.String.eoff = ull;
		}
		if (item->as.Blob.eoff == 0) {
			item->as.Blob.flags |= CPBI_F_LAST;
			break;
		}
	}
	return res;
}
