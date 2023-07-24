#include "chainpack.h"
#include <limits.h>

#ifndef __BYTE_ORDER__
#error We use __BYTE_ORDER__ macro and we need it to be defined
#endif


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
#define CALL(FUNC, ...) \
	do { \
		ssize_t __cnt = FUNC(f, __VA_ARGS__); \
		if (__cnt < 0) \
			return -1; \
		res += __cnt; \
	} while (false)


static ssize_t chainpack_pack_int(FILE *f, int64_t v);
static ssize_t chainpack_pack_buf(FILE *f, const struct cpbuf *buf);


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
			WRITE((uint8_t *)&item->as.Double, sizeof(double), 1);
#endif
			break;
		case CP_ITEM_DECIMAL:
			PUTC(CPS_Decimal);
			CALL(chainpack_pack_int, item->as.Decimal.mantisa);
			CALL(chainpack_pack_int, item->as.Decimal.exponent);
			break;
		case CP_ITEM_BLOB:
			if (item->as.Blob.first) {
				if (item->as.Blob.eoff < 0)
					PUTC(CPS_CString);
				else {
					PUTC(CPS_Blob);
					CALL(_chainpack_pack_uint,
						item->as.Blob.len + item->as.Blob.eoff);
				}
			}
			CALL(chainpack_pack_buf, &item->as.Blob);
			break;
		case CP_ITEM_STRING:
			if (item->as.Blob.first) {
				if (item->as.String.eoff < 0)
					PUTC(CPS_CString);
				else {
					PUTC(CPS_String);
					CALL(_chainpack_pack_uint,
						item->as.String.len + item->as.String.eoff);
				}
			}
			CALL(chainpack_pack_buf, &item->as.String);
			break;
		case CP_ITEM_DATETIME:
			PUTC(CPS_DateTime);
			/* Some arguable optimizations when msec == 0 or TZ_offset == a this
			 * can save byte in packed date-time, but packing scheme is more
			 * complicated.
			 */
			int64_t msecs = item->as.Datetime.msecs - chainpack_epoch_msec;
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


#if defined(__GNUC__) && __GNUC__ >= 4

static int significant_bits_part_length(uint64_t n) {
	int len = 0;
	int llbits = sizeof(long long) * CHAR_BIT;

	if (n == 0)
		return 0;

	if ((llbits < 64) && (n & 0xFFFFFFFF00000000)) {
		len += 32;
		n >>= 32;
	}

	len += llbits - __builtin_clzll(n);

	return len;
}

#else /* Fallback for generic compiler */

// see https://en.wikipedia.org/wiki/Find_first_set#CLZ
static const uint8_t sig_table_4bit[16] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

static int significant_bits_part_length(uint64_t n) {
	int len = 0;

	if (n & 0xFFFFFFFF00000000) {
		len += 32;
		n >>= 32;
	}
	if (n & 0xFFFF0000) {
		len += 16;
		n >>= 16;
	}
	if (n & 0xFF00) {
		len += 8;
		n >>= 8;
	}
	if (n & 0xF0) {
		len += 4;
		n >>= 4;
	}
	len += sig_table_4bit[n];
	return len;
}

#endif /* end of significant_bits_part_length function */

/* Number of bytes needed to encode bitlen */
static size_t bytes_needed(size_t bitlen) {
	size_t cnt;
	if (bitlen <= 28)
		cnt = (bitlen - 1) / 7 + 1;
	else
		cnt = (bitlen - 1) / 8 + 2;
	return cnt;
}

/* UInt
 0 ...  7 bits  1  byte  |0|x|x|x|x|x|x|x|<-- LSB
 8 ... 14 bits  2  bytes |1|0|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB
15 ... 21 bits  3  bytes |1|1|0|x|x|x|x|x| |x|x|x|x|x|x|x|x|
|x|x|x|x|x|x|x|x|<-- LSB 22 ... 28 bits  4  bytes |1|1|1|0|x|x|x|x|
|x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB 29+       bits  5+
bytes |1|1|1|1|n|n|n|n| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|
... <-- LSB n ==  0 ->  4 bytes number (32 bit number) n ==  1 ->  5 bytes
number n == 14 -> 18 bytes number n == 15 -> for future (number of bytes will be
specified in next byte)
*/
static ssize_t pack_uint_data(FILE *f, uint64_t v, size_t bitlen) {
	ssize_t res = 0;
	size_t cnt = bytes_needed(bitlen);
	uint8_t buf[cnt];
	int i;
	for (i = cnt - 1; i >= 0; --i) {
		uint8_t r = v & 0xff;
		buf[i] = r;
		v = v >> 8;
	}

	if (bitlen <= 28) {
		uint8_t mask = 0xf0 << (4 - cnt);
		buf[0] = buf[0] & ~mask;
		mask = mask << 1;
		buf[0] = buf[0] | mask;
	} else
		buf[0] = 0xf0 | (cnt - 5);

	WRITE(buf, cnt, sizeof *buf);
	return res;
}

_Static_assert(sizeof(uint64_t) <= 18, "Limitation of the chainpack packing code");

ssize_t _chainpack_pack_uint(FILE *f, uint64_t v) {
	int bitlen = significant_bits_part_length(v);
	return pack_uint_data(f, v, bitlen);
}

/* Return max bit length >= bit_len, which can be encoded by same number of
 * bytes
 */
static int expand_bitlen(int bitlen) {
	int ret;
	int byte_cnt = bytes_needed(bitlen);
	if (bitlen <= 28)
		ret = byte_cnt * (8 - 1) - 1;
	else
		ret = (byte_cnt - 1) * 8 - 1;
	return ret;
}

/* Int
 0 ...  7 bits  1  byte  |0|s|x|x|x|x|x|x|<-- LSB
 8 ... 14 bits  2  bytes |1|0|s|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB
15 ... 21 bits  3  bytes |1|1|0|s|x|x|x|x| |x|x|x|x|x|x|x|x|
|x|x|x|x|x|x|x|x|<-- LSB 22 ... 28 bits  4  bytes |1|1|1|0|s|x|x|x|
|x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB 29+       bits  5+
bytes |1|1|1|1|n|n|n|n| |s|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|
... <-- LSB n ==  0 ->  4 bytes number (32 bit number) n ==  1 ->  5 bytes
number n == 14 -> 18 bytes number n == 15 -> for future (number of bytes will be
specified in next byte)
*/
static ssize_t chainpack_pack_int(FILE *f, int64_t v) {
	uint64_t num = v < 0 ? -v : v;
	bool neg = (v < 0);

	int bitlen = significant_bits_part_length(num);
	bitlen++; /* add sign bit */
	if (neg) {
		int sign_pos = expand_bitlen(bitlen);
		uint64_t sign_bit_mask = (uint64_t)1 << sign_pos;
		num |= sign_bit_mask;
	}
	return pack_uint_data(f, num, bitlen);
}

static ssize_t chainpack_pack_buf(FILE *f, const struct cpbuf *buf) {
	ssize_t res = 0;

	if (buf->eoff == -1) {
		/* Handle C string */
		for (size_t i = 0; i < buf->len; i++) {
			switch (buf->rbuf[i]) {
				case '\\':
					PUTC('\\');
					PUTC('\\');
					break;
				case '\0':
					PUTC('\\');
					PUTC('0');
					break;
				default:
					PUTC(buf->rbuf[i]);
					break;
			}
		}
		if (buf->last)
			PUTC('\0');
	} else
		WRITE(buf->rbuf, buf->len, 1);

	return res;
}
