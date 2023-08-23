#ifndef SHV_CP_H
#define SHV_CP_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>


enum cp_format {
	CP_ChainPack = 1,
	CP_Cpon,
	CP_Json,
};


enum cp_item_type {
	CP_ITEM_INVALID = 0,
	CP_ITEM_NULL,
	CP_ITEM_BOOL,
	CP_ITEM_INT,
	CP_ITEM_UINT,
	CP_ITEM_DOUBLE,
	CP_ITEM_DECIMAL,
	CP_ITEM_BLOB,
	CP_ITEM_STRING,
	CP_ITEM_DATETIME,
	CP_ITEM_LIST,
	CP_ITEM_MAP,
	CP_ITEM_IMAP,
	CP_ITEM_META,
	CP_ITEM_CONTAINER_END,
};

const char *cp_item_type_str(enum cp_item_type);


/*! Representation of decimal number.
 *
 * The size of mantisa is chosen to cover the biggest numbers the platform can
 * handle automatically (`long long`). The exponent is chosen smaller (`int`)
 * because by supporting at least 16 bits is enough range (consider exponent
 * 10^32767 and estimation of number of atoms in the observeble universe 10^82).
 */
struct cpdecimal {
	long long mantisa;
	int exponent;
};

/*! Convert decimal number to double precision floating point number.
 *
 * @param v: Decimal value to be converted
 * @returns Approximate floating point representation of the decimal number.
 */
double cpdectod(struct cpdecimal v);

/*! Convert double precision floating point number to decimal number.
 *
 * @param v: Floating point number to be converted.
 * @returns Approximate decimal representation.
 */
struct cpdecimal cpdtodec(double v);


/*! Representation of date and time including the time zone offset. */
struct cpdatetime {
	/*! Milliseconds since epoch */
	int64_t msecs;
	/*! Offset for the time zone in minutes */
	int32_t offutc;
};

/*! Convert CP date and time representation to the C's time.
 *
 * @param v: Date and time to be converted.
 * @returns structure tm with date and time set.
 */
struct tm cpdttotm(struct cpdatetime v);

/*! Convert C's time to CP date and time representation.
 *
 * @param v: Date and time to be converted.
 * @returns structure cpdatetime with date and time set.
 */
struct cpdatetime cptmtodt(struct tm v);


/*! Signaling of first and last block. You should set both to `true` if this
 * is the only block to be packed. If you plan to append additional chunks
 * you should set on first iteration only `first` and to terminate you need
 * to set `last`.
 */

#define CPBI_F_FIRST (1 << 0)
#define CPBI_F_LAST (1 << 1)
#define CPBI_F_STREAM (1 << 2)
#define CPBI_F_HEX (1 << 3)
#define CPBI_F_SINGLE (CPBI_F_FIRST | CPBI_F_LAST)

/*
 */
struct cpbufinfo {
	// TODO we need to allow documented data skip without providing buffer
	/*! Number of valid bytes in `buf` (`rbuf`, `chr` and `rchr`).
	 *
	 * The special case is when you invoke packer just to get packed size (that
	 * is `FILE` is `NULL`). In such case buffer is not accessed and thus it can
	 * be `NULL` and thus this only says number of bytes to be packed.
	 */
	size_t len;
	/*! Number of bytes not yet unpacked or provided in buffer for packing.
	 *
	 * Packing: Use this to inform about the full length. The real length of
	 * data to be packed is anything that was packed so far, plus `len` and
	 * `eoff`.
	 *
	 * Unpacking: Set and modified by unpacker. Do not modify it when unpacking.
	 * In case `CPBI_F_STREAM` is not set in `flags` then you can use it to get
	 * a full size on first unpack (`flags` contains `CPBI_F_FIRST`).
	 *
	 * The size is chosen to be at minimum 32 bits and this we should be able to
	 * for sure handle almost strings with 4G size. We are talking here about a
	 * single message. Messages with such a huge size should be broker to the
	 * separate ones and this we do not have to support longer strings.
	 */
	unsigned long eoff;
	/*! Bitwise combination of `VPBI_F_*` flags. */
	uint8_t flags;
};

struct cpitem {
	enum cp_item_type type;
	/*! foo
	 */
	union cpitem_as {
		struct cpbufinfo String;
		struct cpbufinfo Blob;
		struct cpdatetime Datetime;
		struct cpdecimal Decimal;
		unsigned long long UInt;
		long long Int;
		double Double;
		bool Bool;
	} as; // TODO possibly just use anonymous union
	/*! Pointer to the buffer with data chunk.
	 *
	 * It is defined both as `const` as well as modifiable. The pack functions
	 * use `buf` while unpack functions use `rbuf`. Be aware that this can
	 * allow write to the buffer marked as `const` if you pass it to unpack
	 * function.
	 *
	 * The `chr` and `rchr` are additionally provided just to allow easy
	 * assignment of the strings without changing type (which would be required
	 * due to change of the sign).
	 */
	union cpitem_buf {
		uint8_t *buf;
		const uint8_t *rbuf;
		char *chr;
		const char *rchr;
	};
	/*! Size of the `buf` (`rbuf`, `chr` and `rchr`). This is used only by
	 * unpacking functions to know the limit of the buffer.
	 */
	size_t bufsiz;
};


/*! Unpack next item from Chainpack.
 */
ssize_t chainpack_pack(FILE *, const struct cpitem *) __attribute__((nonnull(2)));

ssize_t _chainpack_pack_uint(FILE *, unsigned long long v);

size_t chainpack_unpack(FILE *, struct cpitem *) __attribute__((nonnull));

size_t _chainpack_unpack_uint(FILE *, unsigned long long *v, bool *ok);


struct cpon_state {
	const char *indent;
	size_t depth;
	struct cpon_state_ctx {
		enum cp_item_type tp;
		bool first;
		bool even;
		bool meta;
	} * ctx;
	size_t cnt;
	void (*realloc)(struct cpon_state *state);
};

ssize_t cpon_pack(FILE *, struct cpon_state *, const struct cpitem *)
	__attribute__((nonnull(2, 3)));

size_t cpon_unpack(FILE *, struct cpon_state *, struct cpitem *)
	__attribute__((nonnull));


#endif
