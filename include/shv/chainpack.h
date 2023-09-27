/* SPDX-License-Identifier: MIT */
#ifndef SHV_CHAINPACK_H
#define SHV_CHAINPACK_H
/*! @file
 * The basic utilities for working with Chainpack data. This provides
 * definitions and simple operations you need to unpack and pack data in
 * Chainpack format. This is not a most user friendly way to work with
 * Chainpack. It is suggester rather to use [packer and unpacker](./cp.md)
 * instead.
 **/

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

_Static_assert(sizeof(unsigned long long) <= 17, "Limitation of the ChainPack");


enum chainpack_scheme {
	CPS_Null = 128,
	CPS_UInt,
	CPS_Int,
	CPS_Double,
	CPS_Bool,
	CPS_Blob,
	CPS_String,				/* UTF8 encoded string */
	CPS_DateTimeEpoch_depr, /* Deprecated */
	CPS_List,
	CPS_Map,
	CPS_IMap,
	CPS_MetaMap,
	CPS_Decimal,
	CPS_DateTime,
	CPS_CString,
	CPS_BlobChain,

	CPS_FALSE = 253,
	CPS_TRUE = 254,
	CPS_TERM = 255,
};

/*! UTC msec since 2.2. 2018 followed by signed UTC offset in 1/4 hour
 * `Fri Feb 02 2018 00:00:00 == 1517529600 EPOCH`
 *
 * This is offset used to encode the DateTime.
 */
#define CHAINPACK_EPOCH_MSEC (1517529600000);

/*! Check if integer packed in scheme is signed or unsigned.
 *
 * See the documentation for `chainpack_scheme_int` to understand when this is
 * used.
 *
 * Call this only if `c < CPS_Null`.
 *
 * @param c: Chainpack scheme byte.
 * @returns `true` if integer is signed and `false` for unsigned.
 */
static inline bool chainpack_scheme_signed(uint8_t c) {
	return c & 0x40;
}

/*! Convert scheme to the encoded int.
 *
 * The initial 128 values in the scheme are used to pack small integers. This
 * function provides an easy way to receive value from it.
 *
 * Use this only if `c < CPS_Null;` as otherwise it is not integer packed in
 * scheme.
 *
 * This function can be used for both signed and unsigned integers, because only
 * positive integers are packed this way. You can use `chainpack_scheme_signed`
 * to check if it is suppose to one or the other.
 *
 * @param c: Chainpack scheme byte.
 * @returns Integer value packed in the scheme byte.
 */
static inline unsigned chainpack_scheme_uint(uint8_t c) {
	return c & 0x3f;
}

/*! Check if scheme is Int.
 *
 * Integers in Chainpack can be packed either directly in the scheme or as
 * followup data. Thus to identify if it is an Int it is not enough to just use
 * `c == CPS_Int`. This provides all in one check.
 *
 * @param c: Chainpack scheme byte.
 * @returns `true` if it is signed integer, `false` otherwise.
 */
static inline bool chainpack_is_int(uint8_t c) {
	return c == CPS_Int || (c < CPS_Null && chainpack_scheme_signed(c));
}

/*! Check if scheme is UInt.
 *
 * Integers in Chainpack can be packed either directly in the scheme or as
 * followup data. Thus to identify if it is an UInt it is not enough to just
 * use `c == CPS_UInt`. This provides all in one check.
 *
 * @param c: Chainpack scheme byte.
 * @returns `true` if it is unsigned integer, `false` otherwise.
 */
static inline bool chainpack_is_uint(uint8_t c) {
	return c == CPS_Int || (c < CPS_Null && !chainpack_scheme_signed(c));
}

/*! Number of bytes the integer spans.
 *
 * Chainpack integers can take up to 17 bytes. The length is known from the
 * first unpacked byte right after the scheme byte. This extracts this info from
 * that byte and provides it to you.
 *
 * @param c: A first integer data byte.
 * @returns number of bytes the integer spans.
 */
static inline unsigned chainpack_int_bytes(uint8_t c) {
	unsigned mask = 0x80;
	for (unsigned i = 1; mask > 0x0f; i++) {
		if (!(c & mask))
			return i;
		mask >>= 1;
	}
	return (c & 0x0f) + 5;
}

/*! Unsigned integer value packed in the first integer byte.
 *
 * This is companion function ton the `chainpack_int_bytes` because it extracts
 * remaining bits from the first byte.
 *
 * These are the most significant bits and thus to construct the while number
 * you can just use shift left and binary or to add less significant bits from
 * the followup bytes.
 *
 * @param c: A first integer data byte.
 * @param bytes: Number of bytes this integer spans (use
 * `chainpack_int_bytes`).
 * @returns value from the first integer data byte.
 */
static inline unsigned chainpack_uint_value1(uint8_t c, unsigned bytes) {
	if (bytes > 4)
		return 0;
	return (0xff >> bytes) & c;
}

/*! Signed integer value packed in the first integer byte.
 *
 * This is companion function to the `chainpack_int_bytes` because it extracts
 * remaining bits from the first byte.
 *
 * The sign for up to three bytes is handled in this value and thus you directly
 * can get negative number. For greater number of bytes this function returns
 * `0` and the most significant bit in the following byte is sign (tip: read it
 * to `int8_t` and convert to the type you wish it to be).
 *
 * @param c: A first integer data byte.
 * @param bytes: Number of bytes this integer spans (use
 * `chainpack_int_bytes`).
 * @returns value from the first integer data byte.
 */
static inline int chainpack_int_value1(uint8_t c, unsigned bytes) {
	if (bytes > 4)
		return 0;
	return ((0x80 >> bytes & c) ? -1 : 1) * ((0x7f >> bytes) & c);
}


/*! Unsigned integer number packed in the scheme.
 *
 * You can use this only with numbers from 0 to 63.
 *
 * @param num: Number to be packed.
 * @returns Byte with packed number in the scheme byte.
 */
static inline uint8_t chainpack_w_scheme_uint(unsigned num) {
	return num & 0x3f;
}

/*! Integer number packed in the scheme.
 *
 * You can use this only with numbers from 0 to 63.
 *
 * @param num: Number to be packed.
 * @returns byte with packed number in the scheme byte.
 */
static inline uint8_t chainpack_w_scheme_int(int num) {
	return 0x40 | (num & 0x3f);
}

/*! Deduce number of bytes needed to pack unsigned integer number.
 *
 * @param num: Number to be packed.
 * @returns number of bytes needed to fit packed number.
 */
static inline unsigned chainpack_w_uint_bytes(unsigned long long num) {
	/* Note: we rely here on compiler optimizations. This functions is going to
	 * be inlined and for most constants this should be reduced to a single
	 * branch.
	 */
	if ((num & (ULLONG_MAX << 7)) == 0)
		return 1;
	else if ((num & (ULLONG_MAX << 14)) == 0)
		return 2;
	else if ((num & (ULLONG_MAX << 21)) == 0)
		return 3;
	else if ((num & (ULLONG_MAX << 28)) == 0)
		return 4;
	unsigned i;
	unsigned long long mask = ULLONG_MAX << 32;
	for (i = 0; (num & mask) != 0; i++)
		mask <<= 8;
	return 5 + i;
}

/*! Deduce number of bytes needed to pack signed integer number.
 *
 * @param num: Number to be packed.
 * @returns number of bytes needed to fit packed number.
 */
static inline unsigned chainpack_w_int_bytes(long long num) {
	unsigned long long n = num < 0 ? -num : num;
	if ((n & (ULLONG_MAX << 6)) == 0)
		return 1;
	else if ((n & (ULLONG_MAX << 13)) == 0)
		return 2;
	else if ((n & (ULLONG_MAX << 20)) == 0)
		return 3;
	else if ((n & (ULLONG_MAX << 27)) == 0)
		return 4;
	unsigned i;
	unsigned long long mask = ULLONG_MAX << 31;
	for (i = 0; (n & mask) != 0; i++)
		mask <<= 8;
	return 5 + i;
}

/*! First byte of the unsigned integer number in packed format.
 *
 * This byte can contain some of the most significant bits. To pack the whole
 * number just copy all bytes from most significant ones to the least
 * significant ones to the bytes after this one up to number of bytes deduced by
 * `chainpack_w_uint_bytes`.
 *
 * @param num: Number to be packed.
 * @param bytes: Number of bytes this integers spans (use
 * `chainpack_w_uint_bytes).
 * @returns first byte of the number as it should be packed in Chainpack.
 */
static inline uint8_t chainpack_w_uint_value1(
	unsigned long long num, unsigned bytes) {
	if (bytes > 4)
		return 0xf0 | ((bytes - 5) & 0x0f);
	return (0xe0 << (4 - bytes)) | (num >> (8 * (bytes - 1)));
}

/*! First byte of the signed integer number in packed format.
 *
 * This byte can contain some of the most significant bits and sign. To pack the
 * whole number you need to copy all other bytes of positive number from the
 * most to the least significant bytes. For numbers long up to four bytes that
 * is all. Numbers longer than four byte also need to pack sign (as it is not
 * part of the first byte) to the most significant bit.
 *
 * @param num: Number to be packed.
 * @param bytes: Number of bytes this integers spans (use
 * `chainpack_w_int_bytes).
 * @returns first byte of the number as it should be packed in Chainpack.
 */
static inline uint8_t chainpack_w_int_value1(long long num, unsigned bytes) {
	bool neg = num < 0;
	unsigned long long n = neg ? -num : num;
	if (bytes > 4)
		return 0xf0 | ((bytes - 5) & 0x0f);
	return ((neg ? 0xe8 : 0xe0) << (4 - bytes)) | (n >> (8 * (bytes - 1)));
}

#endif
