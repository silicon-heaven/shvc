/* SPDX-License-Identifier: MIT */
#ifndef SHV_CP_H
#define SHV_CP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <assert.h>
#include <time.h>

/**
 * Serialization and deserialization of ChainPack and CPON data formats
 * implemented on top of :c:type:`FILE` stream.
 */


/** Definition of CP formats. */
enum cp_format {
	/** Binary format used in SHV communication. */
	CP_ChainPack = 1,
	/** Human readable format based on JSON used in SHV communication logging
	 * but can be also used for data input and other such use cases.
	 */
	CP_Cpon,
	/** Obsolete and no longer used format in SHV. It is mentioned only for
	 * completeness.
	 */
	CP_Json,
};


/** Enum identifying different items in SHV CP and few utility ones. */
enum cpitem_type {
	/** Invalid or unknown type. Packers ignore this item and unpackers set it
	 * when error is encountered.
	 */
	CPITEM_INVALID = 0,
	/** NULL type used as dummy value in SHV. */
	CPITEM_NULL,
	/** Boolean value stored in `cpitem.as.Bool`
	 * (:c:var:`cpitem.cpitem_as.Bool`).
	 */
	CPITEM_BOOL,
	/** Integer value stored in `cpitem.as.Int` (:c:var:`cpitem.cpitem_as.Int`).
	 */
	CPITEM_INT,
	/** Unsigned integer value stored in `cpitem.as.UInt`
	 * (:c:var:`cpitem.cpitem_as.UInt`)
	 */
	CPITEM_UINT,
	/** Double precision floating number value stored in `cpitem.as.Double`
	 * (:c:var:`cpitem.cpitem_as.Double`).
	 */
	CPITEM_DOUBLE,
	/** Decimal number stored in `cpitem.as.Decimal`
	 * (:c:var:`cpitem.cpitem_as.Decimal`).
	 */
	CPITEM_DECIMAL,
	/** Date and time with UTC offset stored in `cpitem.as.Datetime`
	 * (:c:var:`cpitem.cpitem_as.Datetime`).
	 */
	CPITEM_DATETIME,
	/** Blob is sequence of bytes.
	 *
	 * To receive them you need to provide :c:var:`cpitem.buf`. Unpacker copies
	 * up to the :c:var:`cpitem.bufsiz` bytes to it and stores info about that
	 * in :c:struct:`cpbufinfo` "cpitem.as.Blob". You might need to call
	 * unpacker multiple times to receive all bytes before you can unpack next
	 * item.
	 *
	 * Packer does the opposite. When you are packing you need to point
	 * :c:var:`cpitem.rbuf` to your data and set :c:struct:`cpbufinfo`
	 * "cpitem.as.Blob" appropriately. Do not forget to correctly manage
	 * :c:var:`cpbufinfo.flags` and especially the :c:macro:`CPBI_F_FIRST` and
	 * :c:macro:`CPBI_F_LAST` flags.
	 */
	CPITEM_BLOB,
	/** Sequence of bytes that you should represent as a string.
	 *
	 * To receive them you need to provide :c:var:`cpitem.chr`. Unpacker copies
	 * up to the :c:var:`cpitem.bufsiz` bytes to it and stores info about that
	 * in :c:struct:`cpbufinfo` "cpitem.as.String". You might need to call
	 * unpacker multiple times to receive all bytes before you can unpack next
	 * item.
	 *
	 * Packer does the opposite. When you are packing you need to point
	 * :c:var:`cpitem.rchr` to your data and set :c:struct:`cpbufinfo`
	 * "cpitem.as.String" appropriately.
	 */
	CPITEM_STRING,
	/** Start of the list container. The followup items are part of this list up
	 * to the container end item.
	 */
	CPITEM_LIST,
	/** Start of the map container. The followup items need to be pairs of
	 * string and any other value. Map needs to be terminated with container
	 * end item.
	 *
	 * Containers from start to their respective end, strings and blobs are
	 * considered as a single items even when you need to invoke packer or
	 * unpacker multiple times. Meta container is always considered to be part
	 * of the following item and thus is not an item on its own.
	 */
	CPITEM_MAP,
	/** Start of the integer map container. The followup items need to be pairs
	 * of integer and any single value. Integer map needs to be terminated with
	 * container end item.
	 *
	 * Containers from start to their respective end, strings and blobs are
	 * considered as a single items even when you need to invoke packer or
	 * unpacker multiple times. Meta container is always considered to be part
	 * of the following item and thus is not an item on its own.
	 */
	CPITEM_IMAP,
	/** Special container that can precede any other item. Primarily it is used
	 * in SHV RPC protocol to store message info. The following items need to be
	 * pairs of either string or integer and any single value. Meta needs to be
	 * terminated with container end item.
	 */
	CPITEM_META,
	/** Container end item used to terminate lists, maps and metas.
	 */
	CPITEM_CONTAINER_END,
};

/** Receive string name for the item type.
 *
 * :param tp: Item type the string should be provided for.
 * :return: pointer to the string with name of the type. This is constant
 *   string: can't be freed or modified.
 */
const char *cpitem_type_str(enum cpitem_type tp);


/** Representation of decimal number.
 *
 * The size of mantissa is chosen to cover the biggest numbers the platform can
 * handle automatically (:c:type:`intmax_t`). The exponent is chosen smaller
 * (``int``) because by supporting at least 16 bits is enough range (consider
 * exponent 10^32767 and estimation of number of atoms in the observable
 * universe 10^82).
 */
struct cpdecimal {
	/** Mantissa in ``mantissa * 10^exponent``. */
	intmax_t mantissa;
	/** Exponent in ``mantissa * 10^exponent``. */
	int exponent;
};

/** Normalize decimal number.
 *
 * The normal format for decimal number is where mantissa is as small as
 * possible without loosing precission. In other words unless mantissa is a zero
 * the residue after division 10 must be non-zero.
 *
 * :param v: Decimal number to be normalized.
 */
[[gnu::nonnull]]
void cpdecnorm(struct cpdecimal *v);

/** Modify decimal number to be at given exponent.
 *
 * The lost precision is never reported and value is cropped instead of
 * rounded!
 *
 * The decimal is modified even if ``false`` is returned. It only stops at
 * exponent that still not cause :c:type:`intmax_t` overflow.
 *
 * :param v: Decimal number to be modified.
 * :param exponent: The desired exponent number.
 * :return: ``true`` if set correctly and ``false`` if exponent can't be reached
 *   due to mantissa overflow.
 */
[[gnu::nonnull]]
bool cpdecexp(struct cpdecimal *v, int exponent);

/** Convert decimal number to double precision floating point number.
 *
 * :param v: Decimal value to be converted
 * :return: Approximate floating point representation of the decimal number.
 */
double cpdectod(const struct cpdecimal v);

/** Convert decimal number to integer with given multiplication.
 *
 * This is commonly used if your application works with integers but values sent
 * in SHV are in fractional units (for example you have milliseconds while SHV
 * sends decimal seconds).
 *
 * :param DEC: The decimal to be converted (variable not pointer to it).
 * :param EXP: The exponent of the destination integer.
 */
#define cpdtoi(DEC, EXP, DEST) \
	({ \
		bool __dec_valid = false; \
		typeof(DEST) *__dest = &(DEST); \
		struct cpdecimal __dec = (DEC); \
		if (cpdecexp(&__dec, (EXP))) { \
			*__dest = __dec.mantissa; \
			__dec_valid = __dec.mantissa == *__dest; \
		} \
		__dec_valid; \
	})

/** Convert given mantissa and exponent to decimal.
 *
 * This automatically normalizes the newly created decimal. Otherwise there is
 * no advantage in using this over direct structure initialization.
 *
 * :param MANTISA: The mantissa to be used.
 * :param EXPONENT: The exponent (current multiplication of mantissa).
 * :return: The normalized decimal number.
 */
#define cpitod(MANTISA, EXPONENT) \
	({ \
		struct cpdecimal __dec_res = {.mantissa = MANTISA, .exponent = EXPONENT}; \
		cpdecnorm(&__dec_res); \
		__dec_res; \
	})


/** Representation of date and time including the time zone offset. */
struct cpdatetime {
	/** Milliseconds since epoch */
	int64_t msecs;
	/** Offset for the time zone in minutes */
	int32_t offutc;
};

/** Convert CP date and time representation to the POSIX time.
 *
 * :param v: Date and time to be converted.
 * :return: structure tm with date and time set.
 */
struct tm cpdttotm(struct cpdatetime v);

/** Convert POSIX time to CP date and time representation.
 *
 * :param v: Date and time to be converted.
 * :return: structure cpdatetime with date and time set.
 */
struct cpdatetime cptmtodt(struct tm v);

/** Convert CP date and time representation to the C timespec.
 *
 * The timespect doesn't include local timezone offset and thus you have to
 * specify if you want to apply the offset from UTC to the local time.
 *
 * :param v: Date and time to be converted.
 * :param utc: If time should be returned in UTC or shifted by offset specified
 *   in the Cp date and time.
 * :return: structure timespec with time set.
 */
static inline struct timespec cpdttots(struct cpdatetime v, bool utc) {
	return (struct timespec){
		.tv_sec = (v.msecs / 1000) + (utc ? 0 : v.offutc * 60),
		.tv_nsec = (v.msecs % 1000) * 1000000,
	};
}

/** Convert C timespec to CP date and time representation.
 *
 * :param v: Date and time to be converted.
 * :return: structure cpdatetime with date and time set.
 */
static inline struct cpdatetime cptstodt(struct timespec v) {
	return (struct cpdatetime){.msecs = (v.tv_sec * 1000) + (v.tv_nsec / 1000000)};
}

/** Get the current clock in :c:type:`cpdatetime` format.
 *
 * :return: Current date and time.
 */
struct cpdatetime clock_cpdatetime(void);


/** Errors reported by unpack functions.
 *
 * Pack functions can encounter only :c:enumerator:`CPERR_IO` because they
 * expect sane input and writable output. The result is that unpack functions do
 * not use these errors.
 */
enum cperror {
	/** There is no error (item is invalid because nothing was stored into it)
	 */
	CPERR_NONE = 0,
	/** Stream reported end of file. */
	CPERR_EOF,
	/** Stream reported error.
	 *
	 * Investigate ``errno`` for the error code.
	 */
	CPERR_IO,
	/** Data in stream do not have valid format */
	CPERR_INVALID,
	/** Data in stream had size outside of the value supported by SHVC.
	 *
	 * This applies to integers where SHVC for optimality chooses to use
	 * sometimes int sometimes :c:type:`intmax_t`.
	 */
	CPERR_OVERFLOW,
};

/*! Provides string describing the CP error.
 *
 * :param error: CP error
 * :return: statically allocated error description message.
 */
const char *cperror_str(enum cperror error);

/** Flag used in :c:var:`cpbufinfo.flags` to inform about first block of Blob or
 * String. This is always set when new string or blob is encountered.
 */
#define CPBI_F_FIRST (1 << 0)
/** Flag used in :c:var:`cpbufinfo.flags` to inform about last block of Blob or
 * String. This tells you that no more bytes can be unpacked for the string.
 */
#define CPBI_F_LAST (1 << 1)
/** Flag used in :c:var:`cpbufinfo.flags` to signal string or blob with unknown
 * size upfront.
 */
#define CPBI_F_STREAM (1 << 2)
/** Flag used in :c:var:`cpbufinfo.flags` for CPON blobs. There are two ways to
 * pack blobs in CPON, regular one that uses escape sequences and sequence of
 * hexadecimal numbers. This flag tells packer that it should use hex
 * representation. Unpacker sets this flag to remember that it should interpret
 * data as hex.
 */
#define CPBI_F_HEX (1 << 3)
/** Combination of :c:macro:`CPBI_F_FIRST` and :c:macro:`CPBI_F_LAST`. It is
 * pretty common to use these two together when you are packing short strings or
 * blobs and thus they are provided in this macro.
 */
#define CPBI_F_SINGLE (CPBI_F_FIRST | CPBI_F_LAST)

/** Info about data stored in the :c:struct:`cpitem` buffer. */
struct cpbufinfo {
	/** Number of valid bytes in :c:var:`cpitem.buf`.
	 *
	 * The special case is when you invoke packer just to get packed size (that
	 * is :c:type:`FILE` is ``NULL``). In such case buffer is not accessed and
	 * thus it can be ``NULL`` and thus this only says number of bytes to be
	 * packed.
	 */
	size_t len;
	/** Number of bytes not yet unpacked or provided in buffer for packing.
	 *
	 * Packing: Use this to inform about the full length. The real length of
	 * data to be packed is anything that was packed so far, plus ``len`` and
	 * ``eoff``.
	 *
	 * Unpacking: Set and modified by unpacker. Do not modify it when unpacking.
	 * In case :c:macro:`CPBI_F_STREAM` is not set in :c:var:`cpbufinfo.flags`
	 * then you can use it to get a full size on first unpack
	 * (:c:var:`cpbufinfo.flags` contains :c:macro:`CPBI_F_FIRST`).
	 *
	 * The size is chosen to be at minimum 32 bits and this we should be able to
	 * for sure handle strings with almost 4G size. We are talking here about a
	 * single message. Messages with such a huge size should be broker to the
	 * separate ones and thus we do not have to support longer strings.
	 */
	unsigned long eoff;
	/** Bitwise combination of `CPBI_F_*` flags. */
	uint8_t flags;
};

/** Representation of a single item (or even part of a single item) used for
 * both packing and unpacking.
 */
struct cpitem {
	/** Type of the item. */
	enum cpitem_type type;
	/** Union to access different representations based on the
	 * :c:var:`cpitem.type`.
	 */
	union cpitem_as {
		/** Used to store value for :c:enumerator:`CPITEM_BOOL`. */
		bool Bool;
		/** Used to store value for :c:enumerator:`CPITEM_INT`. */
		intmax_t Int;
		/** Used to store value for :c:enumerator:`CPITEM_UINT`. */
		uintmax_t UInt;
		/** Used to store value for :c:enumerator:`CPITEM_DOUBLE`. */
		double Double;
		/** Used to store value for :c:enumerator:`CPITEM_DECIMAL`. */
		struct cpdecimal Decimal;
		/** Used to store value for :c:enumerator:`CPITEM_DATETIME`. */
		struct cpdatetime Datetime;
		/** Info about binary data for :c:enumerator:`CPITEM_BLOB`. */
		struct cpbufinfo Blob;
		/** Info about string data for :c:enumerator:`CPITEM_STRING`. */
		struct cpbufinfo String;
		/** Info about type of unpack error for :c:enumerator:`CPITEM_INVALID`.
		 */
		enum cperror Error;
	}
	/** Access to the value for most of the types. The only types not handled
	 * through this are :c:enumerator:`CPITEM_STRING` and
	 * :c:enumerator:`CPITEM_BLOB`.
	 */
	as;
	/** Generic access to the buffers. */
	union {
		/** Buffer used to store binary data when unpacking.
		 *
		 * The special case is when :c:var:`bufsiz` is nonzero while this is set
		 * to ``NULL``, in such situation the read bytes are discarded. The
		 * number of discarded bytes is given by :c:var:`bufsiz`.
		 */
		uint8_t *buf;
		/** Buffer used to pass binary data to packer.
		 *
		 * This is provided as an addition to the :c:var:`buf` for packing
		 * operations to allow usage of constant buffers.
		 */
		const uint8_t *rbuf;
		/** Buffer used to store string data when unpacking.
		 *
		 * This is provided as an addition to the :c:var:`buf` for unpacking
		 * strings.
		 */
		char *chr;
		/** Buffer used to pass string data to packer.
		 *
		 * This is provided as an addition to the :c:var:`rbuf` for packing
		 * operations to allow usage of constant buffers.
		 */
		const char *rchr;
	};
	/** Size of the :c:member:`cpitem.buf` (:c:member:`cpitem.rbuf`,
	 * :c:member:`cpitem.chr`, and :c:member:`cpitem.rchr`). This is used only
	 * by unpacking functions to know the limit of the buffer.
	 *
	 * It is common to set this to zero to receive type of the item and only
	 * after that you would set the pointer to the buffer and its size. Make
	 * sure that you set it back to zero afterward to prevent access to the
	 * pointer used previously here.
	 */
	size_t bufsiz;
};

/** Minimal initialization of :c:struct:`cpitem` for unpacking.
 *
 * The structure items are designed to be zero in default to allow
 * initialization with `struct cpitem foo = {}` but in reality only item type,
 * error type and size of the buffer to the zero. Type needs to be set because
 * unpack could get confused if type would be string or blob. Error type needs
 * to be set to signal no previous error. The buffer size needs to be zero to
 * prevent writing to the buffer.
 *
 * :param item: Pointer to the item to be initialized.
 */
static inline void cpitem_unpack_init(struct cpitem *item) {
	item->type = CPITEM_INVALID;
	item->as.Error = CPERR_NONE;
	item->buf = NULL;
	item->bufsiz = 0;
}

/** Extract integer from the item and check if it fits to the destination.
 *
 * ChainPack and CPON support pretty large integer types. That is limited by
 * platform to :c:type:`intmax_t` but in applications it is more common to use
 * ``int`` or other types. By simple assignment the number can just be
 * mangled to some invalid value if it is too big and thus this macro is
 * provided to extract integer from item while it checks for the destination
 * limits.
 *
 * :param ITEM: item from which integer is extract.
 * :param DEST: destination integer variable (not pointer, the variable
 *   directly). It can be unsigned as well as signed and thus it is allowed to
 *   extract signed SHV integer to unsigned variable.
 * :return: ``true`` in case value was :c:enumerator:`CPITEM_INT` and value fits
 *   to the destination, otherwise ``false`` is returned. The destination is not
 *   modified when ``false`` is returned.
 */
#define cpitem_extract_int(ITEM, DEST) \
	({ \
		const struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_INT) { \
			(DEST) = __item->as.Int; \
			__valid = __item->as.Int == (typeof(DEST))__item->as.Int; \
		} \
		__valid; \
	})

/** Extract unsigned integer from the item and check if it fits to the
 * destination.
 *
 * This is variant of :c:macro:`cpitem_extract_int` for unsigned integers.
 * Please see documentation for :c:macro:`cpitem_extract_int` for an
 * explanation.
 *
 * :param ITEM: item from which unsigned integer is extract.
 * :param DEST: destination unsigned integer variable (not pointer, the variable
 *   directly). It can be signed as well as unsigned and this it is allowed to
 *   extract unsigned SHV integer to signed variable.
 * :return: ``true`` in case value was :c:enumerator:`CPITEM_UINT` and value
 *   fits to the destination, otherwise ``false`` is returned. The destination
 *   is not modified when ``false`` is returned.
 */
#define cpitem_extract_uint(ITEM, DEST) \
	({ \
		const struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_UINT) { \
			(DEST) = __item->as.UInt; \
			__valid = __item->as.UInt == (typeof(DEST))__item->as.UInt; \
		} \
		__valid; \
	})

/** Extract boolean from the item.
 *
 * :param ITEM: item from which boolean is extract.
 * :param DEST: destination boolean variable (not pointer, the variable
 *   directly).
 * :return: ``true`` in case value was :c:enumerator:`CPITEM_BOOL` and value
 *   fits to the destination, otherwise ``false`` is returned. The destination
 *   is not modified when ``false`` is returned.
 */
#define cpitem_extract_bool(ITEM, DEST) \
	({ \
		const struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_BOOL) { \
			(DEST) = __item->as.Bool; \
			__valid = true; \
		} \
		__valid; \
	})

/** Extract :c:struct:`cpdatetime` from the item.
 *
 * :param ITEM: item from which :c:struct:`cpdatetime` is extract.
 * :param DEST: destination :c:struct:`cpdatetime` variable (not pointer, the
 *   variable directly).
 * :return: ``true`` in case value was :c:enumerator:`CPITEM_DATETIME`,
 *   otherwise ``false`` is returned. The destination is not modified when
 *   ``false`` is returned.
 */
#define cpitem_extract_datetime(ITEM, DEST) \
	({ \
		const struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_DATETIME) { \
			(DEST) = __item->as.Datetime; \
			__valid = true; \
		} \
		__valid; \
	})

/** Extract :c:struct:`cpdecimal` from the item.
 *
 * :param ITEM: item from which :c:struct:`cpdecimal` is extract.
 * :param DEST: destination :c:struct:`cpdecimal` variable (not pointer, the
 *   variable directly).
 * :return: ``true`` in case value was :c:enumerator:`CPITEM_DECIMAL` and,
 *   otherwise ``false`` is returned. The destination is not modified when
 *   ``false`` is returned.
 */
#define cpitem_extract_decimal(ITEM, DEST) \
	({ \
		const struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_DECIMAL) { \
			(DEST) = __item->as.Decimal; \
			__valid = true; \
		} \
		__valid; \
	})

/** Unpack item from ChainPack data format.
 *
 * :param f: File from which ChainPack bytes are read from.
 * :param item: Item where info about the unpacked item and its value is placed
 *   to.
 * :return: Number of bytes read from ``f``.
 */
[[gnu::nonnull]]
size_t chainpack_unpack(FILE *f, struct cpitem *item);

/** Pack next item to ChainPack data format.
 *
 * :pram f: File to which ChainPack bytes are written to. It can be ``NULL`` and
 *   in such a case packer only calculates number of bytes item would take when
 *   packed.
 * :param item: Item to be packed.
 * :return: Number of bytes written to **f**. On error ``-1`` is returned. Note
 *   that some bytes might have been successfully written before error was
 *   detected. Number of written bytes in case of an error is irrelevant because
 *   packed data is invalid anyway.
 */
[[gnu::nonnull(2)]]
ssize_t chainpack_pack(FILE *f, const struct cpitem *item);


/** State for the CPON packer and unpacker.
 *
 * Compared to the ChainPack, CPON needs additional information because of these
 * issues:
 *
 * * Containers are terminated with either ``}`` or ``]`` based on the type
 *   compared to the unified termination in ChainPack and CP API.
 * * Map and IMap containers use item separators (``,`` and ``:``) where in
 *   ChainPack simple end of an item is the separator.
 *
 * Because of that CPON packer and unpacker needs to remember stack of
 * containers as well as additional flags to decide on the separator to use.
 */
struct cpon_state {
	/** Counter of opened containers. */
	size_t depth;
	/** Container context. */
	struct cpon_state_ctx {
		/** Container type */
		enum cpitem_type tp;
		/** Flag signaling that there is not a single item in the container yet.
		 */
		bool first;
		/** Flag signaling that even number of items were packed to the
		 * container. This decides if ``,`` or ``:`` should be used as
		 * separator.
		 */
		bool even;
		/** Flag signaling that previous item was meta and thus that next item
		 * should not be separated by any separator.
		 */
		bool meta;
	}
		/** Array of container contexts. This array can be preallocated or it
		 * can be dynamically allocated on demand by
		 * :c:var:`cpon_state.realloc`.
		 */
		*ctx;
	/** Size of container contexts array (:c:var:`cpon_state.ctx`).
	 *
	 * This is fixed size if :c:var:`cpon_state.realloc` is ``NULL`` or it has
	 * to be modified by that function.
	 */
	size_t cnt;
	/** Pointer to the function that is called when there is not enough space to
	 * store additional container context. This function needs to reallocate the
	 * current array to increase its size or it can just do nothing but it can't
	 * decrease the array size under the :c:var:`depth`. The new size needs to
	 * be stored in :c:var:`cnt`.
	 *
	 * This field can be set to ``NULL`` and in such case the dynamic allocation
	 * of container contexts won't be possible.
	 */
	void (*realloc)(struct cpon_state *state);
	/** Indent to be used for CPON packer. This is rather a packer configuration
	 * but it is placed in the state because it makes it a convenient way to
	 * pass it consistently to every :c:func:`cpon_pack` call.
	 *
	 * You can set it to ``NULL`` to pack CPON on a singe line.
	 */
	const char *indent;
};

/** Pack next item to CPON data format.
 *
 * The unpacker is strict in CPON format but only if :c:var:`cpon_state.cnt` is
 * more than :c:var:`cpon_state.depth`. In other words if unpacker can't
 * remember context it will disregard difference between ``:`` and ``,`` as well
 * as ``}`` and ``]``.
 *
 * :param f: File from which CPON bytes are read from.
 * :param state: CPON state (can't be shared with packer!) that preserves
 *   context information between function calls.
 * :param item: Item where info about the unpacked item and its value is placed
 *   to.
 * :return: Number of bytes read from **f**.
 */
[[gnu::nonnull]]
size_t cpon_unpack(FILE *f, struct cpon_state *state, struct cpitem *item);

/** Unpack next item from CPON data format.
 *
 * The packer can reliably pack only if :c:var:`cpon_state.cnt` is more than
 * :c:var:`cpon_state.depth`. Any container that is beyond that limit is ignored
 * and replaced by sequence `...` (invalid in CPON).
 *
 * :param f: File to which CPON bytes are written to. It can be ``NULL`` and in
 *   such a case packer only calculates number of bytes item would take when
 *   packed. Be aware that this operation updates CPON state and thus you can't
 *   just simply call it with ``NULL`` and then call it with same item with
 *   :c:type:`FILE`.
 * :param state: CPON state (can't be shared with unpacker!) that preserves
 *   context information between function calls.
 * :param item: Item to be packed.
 * :returns: Number of bytes written to **f**. On error ``-1`` is returned. Note
 *   that some bytes might have been successfully written before error was
 *   detected. Number of written bytes in case of an error is irrelevant because
 *   packed data is invalid anyway.
 */
[[gnu::nonnull(2, 3)]]
ssize_t cpon_pack(FILE *f, struct cpon_state *state, const struct cpitem *item);


#endif
