#ifndef SHV_CP_H
#define SHV_CP_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>


/*! Definition of CP formats. */
enum cp_format {
	/*! Binary format used in SHV communication. */
	CP_ChainPack = 1,
	/*! Human readable format based on JSON used in SHV communication logging
	 * but can be also used for data input and other such use cases.
	 */
	CP_Cpon,
	/*! Obsolete and no longer used format in SHV. It is mentioned only for
	 * completeness.
	 */
	CP_Json,
};


/*! Enum identifying different items in SHV CP and few utility ones. */
enum cpitem_type {
	/*! Invalid or unknown type. Packers ignore this item and unpackers set it
	 * when error is encountered.
	 */
	CPITEM_INVALID = 0,
	/*! NULL type used as dummy value in SHV. */
	CPITEM_NULL,
	/*! Boolean value stored in `cpitem.as.Bool` (@ref cpitem.cpitem_as.Bool). */
	CPITEM_BOOL,
	/*! Integer value stored in `cpitem.as.Int` (@ref cpitem.cpitem_as.Int). */
	CPITEM_INT,
	/*! Unsigned integer value stored in `cpitem.as.UInt` (@ref
	 * cpitem.cpitem_as.UInt) */
	CPITEM_UINT,
	/*! Double precision floating number value stored in `cpitem.as.Double`
	 * (@ref cpitem.cpitem_as.Double). */
	CPITEM_DOUBLE,
	/*! Decimal number stored in `cpitem.as.Decimal` (@ref
	 * cpitem.cpitem_as.Decimal). */
	CPITEM_DECIMAL,
	/*! Date and time with UTC offset stored in `cpitem.as.Datetime` (@ref
	 * cpitem.cpitem_as.Datetime). */
	CPITEM_DATETIME,
	/*! Blob is sequence of bytes.
	 *
	 * To receive them you need to provide @ref cpitem.cpitem_buf.buf. Unpacker
	 * copies up to the @ref cpitem.bufsiz bytes to it and stores info about
	 * that in @ref cpbufinfo "cpitem.as.Blob". You might need to call
	 * unpacker multiple times to receive all bytes before you can unpack next
	 * item.
	 *
	 * Packer does the opposite. When you are packing you need to point @ref
	 * cpitem.cpitem_buf.rbuf to your data and set @ref cpbufinfo
	 * "cpitem.as.Blob" appropriately. Do not forget to correctly manage @ref
	 * cpbufinfo.flags and especially the @ref CPBI_F_FIRST and @ref CPBI_F_LAST
	 * flags.
	 */
	CPITEM_BLOB,
	/*! Sequence of bytes that you should represent as a string.
	 *
	 * To receive them you need to provide @ref cpitem.cpitem_buf.chr. Unpacker
	 * copies up to the @ref cpitem.bufsiz bytes to it and stores info about
	 * that in @ref cpbufinfo "cpitem.as.String". You might need to call
	 * unpacker multiple times to receive all bytes before you can unpack next
	 * item.
	 *
	 * Packer does the opposite. When you are packing you need to point @ref
	 * cpitem.cpitem_buf.rchr to your data and set @ref cpbufinfo
	 * "cpitem.as.String" appropriately.
	 */
	CPITEM_STRING,
	/*! Start of the list container. The followup items are part of this list up
	 * to the container end item.
	 */
	CPITEM_LIST,
	/*! Start of the map container. The followup items need to be pairs of
	 * string and any other value. Map needs to be terminated with container
	 * end item.
	 *
	 * Containers from start to their respective end, strings and blobs are
	 * considered as a single items even when you need to invoke packer or
	 * unpacker multiple times. Meta container is always considered to be part
	 * of the following item and thus is not an item on its own.
	 */
	CPITEM_MAP,
	/*! Start of the integer map container. The followup items need to be pairs
	 * of integer and any single value. Integer map needs to be terminated with
	 * container end item.
	 *
	 * Containers from start to their respective end, strings and blobs are
	 * considered as a single items even when you need to invoke packer or
	 * unpacker multiple times. Meta container is always considered to be part
	 * of the following item and thus is not an item on its own.
	 */
	CPITEM_IMAP,
	/*! Special container that can precede any other item. Primarily it is used
	 * in SHV RPC protocol to store message info. The following items need to be
	 * pairs of either string or integer and any single value. Meta needs to be
	 * terminated with container end item.
	 */
	CPITEM_META,
	/*! Container end item used to terminate lists, maps and metas.
	 */
	CPITEM_CONTAINER_END,
	/*! The special item type that signals raw operation on the data. Sometimes
	 * it is required to bypass the parser (because there are binary data
	 * inserted in the data) or it is just more efficient to copy data without
	 * interpreting them. For that this special type is available.
	 *
	 * For unpacker you need to provide pointer to the writable buffer in
	 * @ref cpitem.cpitem_buf.buf and unpacker will store up to @ref
	 * cpitem.bufsiz bytes in it.  Number of valid bytes will be stored in
	 * @ref cpbufinfo.len "cpitem.as.Blob.len".
	 *
	 * For packer you need to provide pointer to the data in @ref
	 * cpitem.cpitem_buf.rbuf and packer will write @ref cpbufinfo.len
	 * "cpitem.as.Blob.len" bytes without interpreting them.
	 */
	CPITEM_RAW = 256,
};

/*! Receive string name for the item type.
 *
 * @param tp: Item type the string should be provided for.
 * @returns pointer to the string with name of the type. This is constant
 * string: can't be freed or modified.
 */
const char *cpitem_type_str(enum cpitem_type tp);


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

/*! Normalize decimal number.
 *
 * The normal format for decimal number is where mantisa is as small as possible
 * without loosing precission. In other words unless mantisa is a zero the
 * residue after division 10 must be non-zero.
 *
 * @param v: Decimal number to be normalized.
 */
void cpdecnorm(struct cpdecimal *v) __attribute__((nonnull));

/*! Compare two decimal numbers.
 *
 * The input must be normalized, otherwise assert is tripped.
 *
 * @returns a positive number if ``A`` is greater than ``B``, a negative number
 *   ``A`` is less than ``B`` and zero if ``A`` is equal to ``B``.
 */
#define cpdeccmp(A, B) \
	({ \
		assert(A->mantisa % 10 || A->mantisa == 0); \
		assert(B->mantisa % 10 || B->mantisa == 0); \
		long long res = A->exponent - B->exponment; \
		if (res == 0) \
			res = A->mantisa - B->mantisa; \
		res; \
	})

/*! Convert decimal number to double precision floating point number.
 *
 * @param v: Decimal value to be converted
 * @returns Approximate floating point representation of the decimal number.
 */
double cpdectod(const struct cpdecimal v);

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

/*! Convert CP date and time representation to the POSIX time.
 *
 * @param v: Date and time to be converted.
 * @returns structure tm with date and time set.
 */
struct tm cpdttotm(struct cpdatetime v);

/*! Convert POSIX time to CP date and time representation.
 *
 * @param v: Date and time to be converted.
 * @returns structure cpdatetime with date and time set.
 */
struct cpdatetime cptmtodt(struct tm v);


/*! Errors reported by unpack functions.
 *
 * Pack functions can encounter only @ref CPERR_IO because they expect sane
 * input and writable output. The result is that unpack functions do not use
 * these errors.
 */
enum cperror {
	/*! There is no error (item is invalid because nothing was stored into it)
	 */
	CPERR_NONE = 0,
	/*! Stream reported end of file. */
	CPERR_EOF,
	/*! Stream reported error.
	 *
	 * Investigate `errno` for the error code.
	 */
	CPERR_IO,
	/*! Data in stream do not have valid format */
	CPERR_INVALID,
	/*! Data in stream had size outside of the value supported by SHVC.
	 *
	 * This applies to numeric as well as too long strings or blobs.
	 */
	CPERR_OVERFLOW,
};

/*! Flag used in @ref cpbufinfo.flags to inform about first block of Blob or
 * String. This is always set when new string or blob is encountered.
 */
#define CPBI_F_FIRST (1 << 0)
/*! Flag used in @ref cpbufinfo.flags to inform about last block of Blob or
 * String. This tells you that no more bytes can be unpacked for the string.
 */
#define CPBI_F_LAST (1 << 1)
/*! Flag used in @ref cpbufinfo.flags to signal string or blob with unknown
 * size upfront.
 */
#define CPBI_F_STREAM (1 << 2)
/*! Flag used in @ref cpbufinfo.flags for CPON blobs. There are two ways to
 * pack blobs in CPON, regular one that uses escape sequences and sequence of
 * hexadecimal numbers. This flag tells packer that it should use hex
 * representation. Unpacker sets this flag to remember that it should interpret
 * data as hex.
 */
#define CPBI_F_HEX (1 << 3)
/*! Combination of @ref CPBI_F_FIRST and CPBI_F_LAST. It is pretty common to use
 * these two together when you are packing short strings or blobs and thus they
 * are provided in this macro.
 */
#define CPBI_F_SINGLE (CPBI_F_FIRST | CPBI_F_LAST)

/*! Info about data stored in the @ref cpitem buffer. */
struct cpbufinfo {
	/*! Number of valid bytes in @ref cpitem.cpitem_buf.
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
	 * In case @ref CPBI_F_STREAM is not set in @ref cpbufinfo.flags then you
	 * can use it to get a full size on first unpack (@ref cpbufinfo.flags
	 * contains @ref CPBI_F_FIRST).
	 *
	 * The size is chosen to be at minimum 32 bits and this we should be able to
	 * for sure handle strings with almost 4G size. We are talking here about a
	 * single message. Messages with such a huge size should be broker to the
	 * separate ones and thus we do not have to support longer strings.
	 */
	unsigned long eoff;
	/*! Bitwise combination of `CPBI_F_*` flags. */
	uint8_t flags;
};

/*! Representation of a single item (or even part of a single item) used for
 * both packing and unpacking.
 */
struct cpitem {
	/*! Type of the item. */
	enum cpitem_type type;
	/*! TODO
	 */
	union cpitem_as {
		/*! Used to store value for @ref CPITEM_BOOL. */
		bool Bool;
		/*! Used to store value for @ref CPITEM_INT. */
		long long Int;
		/*! Used to store value for @ref CPITEM_UINT. */
		unsigned long long UInt;
		/*! Used to store value for @ref CPITEM_DOUBLE. */
		double Double;
		/*! Used to store value for @ref CPITEM_DECIMAL. */
		struct cpdecimal Decimal;
		/*! Used to store value for @ref CPITEM_DATETIME. */
		struct cpdatetime Datetime;
		/*! Info about binary data for @ref CPITEM_BLOB. */
		struct cpbufinfo Blob;
		/*! Info about string data for @ref CPITEM_STRING. */
		struct cpbufinfo String;
		/*! Info about type of unpack error for @ref CPITEM_INVALID. */
		enum cperror Error;
	}
	/*! Wtf */
	as; // TODO possibly just use anonymous union
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
	 *
	 * The special case is when you set buffer to `NULL` while `bufsiz != 0`,
	 * this discards up to `bufsiz` bytes instead of copying. This is used to
	 * skip data.
	 */
	union cpitem_buf {
		/*! Buffer used to store binary data when unpacking. */
		uint8_t *buf;
		/*! Buffer used to receive binary data when packing. */
		const uint8_t *rbuf;
		/*! Buffer used to store string data when unpacking. */
		char *chr;
		/*! Buffer used to receive string data when packing. */
		const char *rchr;
	};
	/*! Size of the @ref cpitem_buf.buf (@ref cpitem_buf.rbuf, @ref
	 * cpitem_buf.chr and @ref cpitem_buf.rchr). This is used only by unpacking
	 * functions to know the limit of the buffer.
	 *
	 * It is common to set this to zero to receive type of the item and only
	 * after that you would set the pointer to the buffer and its size.
	 */
	size_t bufsiz;
};

/*! Minimal initialization of @ref cpitem for unpacking.
 *
 * The structure items are designed to be zero in default to allow
 * initialization with `struct cpitem foo = {}` but in reality only item type,
 * error type and size of the buffer to the zero. Type needs to be set because
 * unpack could get confused if type would be string or blob. Error type needs
 * to be set to signal no previous error. The buffer size needs to be zero to
 * prevent writing to the buffer.
 *
 * @param item: Pointer to the item to be initialized.
 */
static inline void cpitem_unpack_init(struct cpitem *item) {
	item->type = CPITEM_INVALID;
	item->as.Error = CPERR_NONE;
	item->buf = NULL;
	item->bufsiz = 0;
}


/*! Unpack item from ChainPack data format.
 *
 * @returns Number of bytes read from @ref f.
 */
size_t chainpack_unpack(FILE *f, struct cpitem *item) __attribute__((nonnull));

/*! Pack next item to ChainPack data format.
 *
 * @returns Number of bytes written to @ref f. On error it returns value of
 * zero. Note that some bytes might have been successfully written before error
 * was detected. Number of written bytes in case of an error is irrelevant
 * because packed data is invalid anyway. Be aware that some inputs might
 * inevitably produce no output (strings and blobs that are not first or last
 * and have zero length produce no output).
 */
size_t chainpack_pack(FILE *f, const struct cpitem *item)
	__attribute__((nonnull(2)));


struct cpon_state {
	const char *indent;
	size_t depth;
	struct cpon_state_ctx {
		enum cpitem_type tp;
		bool first;
		bool even;
		bool meta;
	} * ctx;
	size_t cnt;
	void (*realloc)(struct cpon_state *state);
};

/*! Pack next item to CPON data format.
 *
 * @returns Number of bytes read from @ref f.
 */
size_t cpon_unpack(FILE *f, struct cpon_state *state, struct cpitem *item)
	__attribute__((nonnull));

/*! Unpack next item from CPON data format.
 *
 * @returns Number of bytes written to @ref f. On error it returns value of
 * zero. Note that some bytes might have been successfully written before error
 * was detected. Number of written bytes in case of an error is irrelevant
 * because packed data is invalid anyway. Be aware that some inputs might
 * inevitably produce no output (strings and blobs that are not first or last
 * and have zero length produce no output).
 */
size_t cpon_pack(FILE *f, struct cpon_state *state, const struct cpitem *item)
	__attribute__((nonnull(2, 3)));


#endif
