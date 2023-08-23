#include <shv/cp.h>
#include <shv/chainpack.h>

#ifndef __BYTE_ORDER__
#error We use __BYTE_ORDER__ macro and we need it to be defined
#endif


#define PUTC(V) \
	do { \
		uint8_t __v = (V); \
		if (f && fputc(__v, f) != __v) \
			return -1; \
		res++; \
	} while (false)
#define WRITE(V, SIZ) \
	do { \
		size_t __siz = SIZ; \
		if (f) { \
			if (fwrite((V), 1, __siz, f) != __siz) \
				return -1; \
		} \
		res += __siz; \
	} while (false)
#define CALL(FUNC, ...) \
	do { \
		ssize_t __cnt = FUNC(f, __VA_ARGS__); \
		if (__cnt < 0) \
			return -1; \
		res += __cnt; \
	} while (false)


static ssize_t chainpack_pack_int(FILE *f, int64_t v);


ssize_t chainpack_pack(FILE *f, const struct cpitem *item) {
	ssize_t res = 0;

	switch (item->type) {
		/* We pack invalid as NULL to ensure that we pack at least somethuing */
		case CP_ITEM_INVALID:
		case CP_ITEM_NULL:
			PUTC(CPS_Null);
			break;
		case CP_ITEM_BOOL:
			PUTC(item->as.Bool ? CPS_TRUE : CPS_FALSE);
			break;
		case CP_ITEM_INT:
			if (item->as.Int >= 0 && item->as.Int < 64)
				PUTC((item->as.Int % 64) + 64);
			else {
				PUTC(CPS_Int);
				CALL(chainpack_pack_int, item->as.Int);
			}
			break;
		case CP_ITEM_UINT:
			if (item->as.UInt < 64)
				PUTC(item->as.Int % 64);
			else {
				PUTC(CPS_UInt);
				CALL(_chainpack_pack_uint, item->as.UInt);
			}
			break;
		case CP_ITEM_DOUBLE:
			PUTC(CPS_Double);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			const uint8_t *_b = (const uint8_t *)&item->as.Double;
			for (ssize_t i = sizeof(double) - 1; i >= 0; i--)
				PUTC(b[i]);
#else
			WRITE((uint8_t *)&item->as.Double, sizeof(double));
#endif
			break;
		case CP_ITEM_DECIMAL:
			PUTC(CPS_Decimal);
			CALL(chainpack_pack_int, item->as.Decimal.mantisa);
			CALL(chainpack_pack_int, item->as.Decimal.exponent);
			break;
		case CP_ITEM_BLOB:
			if (item->as.Blob.flags & CPBI_F_FIRST) {
				if (item->as.Blob.flags & CPBI_F_STREAM) {
					PUTC(CPS_BlobChain);
				} else {
					PUTC(CPS_Blob);
					CALL(_chainpack_pack_uint,
						item->as.Blob.len + item->as.Blob.eoff);
				}
			}
			if (item->as.Blob.len) {
				if (item->as.Blob.flags & CPBI_F_STREAM)
					CALL(_chainpack_pack_uint, item->as.Blob.len);
				WRITE(item->rbuf, item->as.Blob.len);
			}
			if (item->as.Blob.flags & CPBI_F_STREAM &&
				item->as.Blob.flags & CPBI_F_LAST)
				PUTC(0);
			break;
		case CP_ITEM_STRING:
			if (item->as.String.flags & CPBI_F_FIRST) {
				if (item->as.String.flags & CPBI_F_STREAM)
					PUTC(CPS_CString);
				else {
					PUTC(CPS_String);
					CALL(_chainpack_pack_uint,
						(int64_t)item->as.String.len + item->as.String.eoff);
				}
			}
			WRITE(item->rbuf, item->as.Blob.len);
			if (item->as.Blob.flags & CPBI_F_STREAM &&
				item->as.Blob.flags & CPBI_F_LAST)
				PUTC('\0');
			break;
		case CP_ITEM_DATETIME:
			PUTC(CPS_DateTime);
			/* Some arguable optimizations when msec == 0 or TZ_offset == a this
			 * can save byte in packed date-time, but packing scheme is more
			 * complicated.
			 */
			int64_t msecs = item->as.Datetime.msecs - CHAINPACK_EPOCH_MSEC;
			int offset = (item->as.Datetime.offutc / 15) & 0x7F;
			int ms = (int)(msecs % 1000);
			if (ms == 0)
				msecs /= 1000;
			if (offset != 0) {
				msecs *= 128;
				msecs |= offset;
			}
			msecs *= 4;
			if (offset != 0)
				msecs |= 1;
			if (ms == 0)
				msecs |= 2;
			CALL(chainpack_pack_int, msecs);
			break;
		case CP_ITEM_LIST:
			PUTC(CPS_List);
			break;
		case CP_ITEM_MAP:
			PUTC(CPS_Map);
			break;
		case CP_ITEM_IMAP:
			PUTC(CPS_IMap);
			break;
		case CP_ITEM_META:
			PUTC(CPS_MetaMap);
			break;
		case CP_ITEM_CONTAINER_END:
			PUTC(CPS_TERM);
			break;
	}
	return res;
}

ssize_t _chainpack_pack_uint(FILE *f, unsigned long long v) {
	ssize_t res = 0;
	unsigned bytes = chainpack_w_uint_bytes(v);
	uint8_t buf[bytes];
	buf[0] = chainpack_w_uint_value1(v, bytes);
	for (unsigned i = 1; i < bytes; i++)
		buf[i] = 0xff & (v >> (8 * (bytes - i - 1)));
	WRITE(buf, bytes);
	return res;
}

static ssize_t chainpack_pack_int(FILE *f, int64_t v) {
	ssize_t res = 0;
	unsigned bytes = chainpack_w_int_bytes(v);
	uint8_t buf[bytes];
	buf[0] = chainpack_w_int_value1(v, bytes);
	uint64_t uv = v < 0 ? -v : v;
	for (unsigned i = 1; i < bytes; i++)
		buf[i] = 0xff & (uv >> (8 * (bytes - i - 1)));
	if (bytes > 4 && v < 0)
		buf[1] |= 0x80;
	WRITE(buf, bytes);
	return res;
}
