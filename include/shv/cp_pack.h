/* SPDX-License-Identifier: MIT */
#ifndef SHV_CP_PACK_H
#define SHV_CP_PACK_H
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <shv/cp.h>

/**
 * Generic packer API with utility functions to pack data more easilly.
 */

/** Definition of function that provides generic packer.
 *
 * This function is called to pack next item and all its context, including
 * the output, is provided by **ptr**.
 *
 * This abstraction allows us to work with any packer in the same way and it
 * also provides a way to overlay additional handling on top of the low level
 * packer (such as logging).
 *
 * :param ptr: Pointer to the context information that is pointer to the
 *   :c:type:`cp_pack_t`.
 * :param item: Item to be packed.
 * :return: ``true`` in case packing was sucesfull and ``false`` otherwise. The
 *   common way to diagnose such error is not provided. If we are talking about
 *   bare ChainPack and CPON packers it is going to mean either stream error or
 *   EOF. The generic meaning is that additional packing won't be possible when
 *   ``false`` is returned.
 */
typedef bool (*cp_pack_func_t)(void *ptr, const struct cpitem *item);
/** Generic packer.
 *
 * This is pointer to the function pointer that implements packing. The function
 * is called by dereferencing this generic packer and the pointer to it is
 * passed to the function as the first argument. This double pointer provides
 * you a way to store any context info side by pointer to the pack function.
 *
 * To understand this you can look into the :c:struct:`cp_pack_chainpack` and
 * :c:struct:`cp_pack_cpon` definitions. They have :c:type:`cp_pack_func_t` as a
 * first field and thus this function gets pointer to the structure in the first
 * argument.
 */
typedef cp_pack_func_t *cp_pack_t;


/** Handle for the ChainPack generic packer. */
struct cp_pack_chainpack {
	/** Generic packer function. */
	cp_pack_func_t func;
	/** File object used in :c:func:`chainpack_pack` calls. */
	FILE *f;
};

/** Initialize :c:struct:`cp_pack_chainpack`.
 *
 * The initialization only fills in :c:var:`cp_pack_chainpack.func` and sets
 * :c:var:`cp_pack_chainpack.f` to the **f** passed to it. There is no need for
 * a special resource deallocation afterward.
 *
 * :param pack: Pointer to the handle to be initialized.
 * :param f: File used to write ChainPack bytes.
 * :return: Generic packer.
 */
[[gnu::nonnull]]
cp_pack_t cp_pack_chainpack_init(struct cp_pack_chainpack *pack, FILE *f);


/** Handle for the CPON generic packer. */
struct cp_pack_cpon {
	/** Generic packer function. */
	cp_pack_func_t func;
	/** File object used in :c:func:`cpon_pack` calls. */
	FILE *f;
	/** Context information state for the CPON.
	 *
	 * Function :c:func:`cp_pack_cpon_init` will prepare this state for the
	 * unconstrained allocation. If you prefer to limit the depth you can change
	 * the :c:var:`cpon_state.realloc` to your own implementation.
	 *
	 * After generic packer end of use you need to free :c:var:`cpon_state.ctx`!
	 */
	struct cpon_state state;
};

/** Initialize :c:struct:`cp_pack_cpon`.
 *
 * The initialization fills in :c:var:`cp_pack_cpon.func`, sets
 * :c:var:`cp_pack_cpon.f` to the **f** passed to it, initializes
 * :c:struct:`cpon_state` to zeroes and sets :c:var:`cpon_state.realloc` to
 * unconstrained allocator and :c:var:`cpon_state.indent` to the **indent**
 * passed to it.
 *
 * Remember to release resources allocated by :c:var:`cpon_state.ctx`!
 *
 * :param pack: Pointer to the handle to be initialized.
 * :param f: File used to write CPON bytes.
 * :param indent: Indent to be used for CPON output. You can pass ``NULL`` to
 *   get CPON on a single line.
 * :return: Generic packer.
 */
[[gnu::nonnull(1, 2)]]
cp_pack_t cp_pack_cpon_init(struct cp_pack_cpon *pack, FILE *f, const char *indent);


/** Pack item with generic packer.
 *
 * The calling is described in :c:type:`cp_pack_func_t`.
 *
 * :param PACK: Generic packer to be used for packing.
 * :param ITEM: Item to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
#define cp_pack(PACK, ITEM) \
	({ \
		cp_pack_t __pack = PACK; \
		(*__pack)(__pack, (ITEM)); \
	})

/** Check the packing success.
 *
 * This doesn't pack anything; the packing is attempted with type
 * :c:enumerator:`CPITEM_INVALID` and :c:var:`cpitem.cpitem_as.Error` set to
 * :c:enumerator:`CPERR_NONE` which packs no data but still checks if packer is
 * in error state or not.
 *
 * The common usage for packer is to just blindly pack simple types. If some of
 * that packing results into an error then any subsequent packing attempts
 * must result into an error as well (unless there is some out of bound error
 * reset performed). Thus it is easier most of the time to just blindly call
 * packing functions and detect error only at the end.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_success(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_INVALID;
	i.as.Error = CPERR_NONE;
	return cp_pack(pack, &i);
}

/** Pack *Null* to the generic packer.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_null(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_NULL;
	return cp_pack(pack, &i);
}

/** Pack *Boolean* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param v: Boolean value to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_bool(cp_pack_t pack, bool v) {
	struct cpitem i;
	i.type = CPITEM_BOOL;
	i.as.Bool = v;
	return cp_pack(pack, &i);
}

/** Pack *Int* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param v: Integer value to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_int(cp_pack_t pack, long long v) {
	struct cpitem i;
	i.type = CPITEM_INT;
	i.as.Int = v;
	return cp_pack(pack, &i);
}

/** Pack *UInt* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param v: Uinsigned integer value to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_uint(cp_pack_t pack, unsigned long long v) {
	struct cpitem i;
	i.type = CPITEM_UINT;
	i.as.UInt = v;
	return cp_pack(pack, &i);
}

/** Pack *Double* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param v: Float point number value to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_double(cp_pack_t pack, double v) {
	struct cpitem i;
	i.type = CPITEM_DOUBLE;
	i.as.Double = v;
	return cp_pack(pack, &i);
}

/** Pack *Decimal* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param v: Decimal number value to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_decimal(cp_pack_t pack, struct cpdecimal v) {
	struct cpitem i;
	i.type = CPITEM_DECIMAL;
	i.as.Decimal = v;
	return cp_pack(pack, &i);
}

/** Pack *Decimal* stored as integer multiple to the generic packer.
 *
 * :param pack: Generic packer.
 * :param mantissa: The mantissa of the decimal number.
 * :param exponent: The exponent (current multiplication of mantissa).
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_decimal_int(
	cp_pack_t pack, intmax_t mantissa, int exponent) {
	return cp_pack_decimal(pack, cpitod(mantissa, exponent));
}

/** Pack *Datetime* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param v: Date and time value to be packed.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_datetime(cp_pack_t pack, struct cpdatetime v) {
	struct cpitem i;
	i.type = CPITEM_DATETIME;
	i.as.Datetime = v;
	return cp_pack(pack, &i);
}

/** Pack *Blob* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param buf: Buffer to be packed as a signle blob.
 * :param len: Size of the **buf** (valid number of bytes to be packed).
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_blob(cp_pack_t pack, const uint8_t *buf, size_t len) {
	struct cpitem i;
	i.type = CPITEM_BLOB;
	i.rbuf = buf;
	i.as.Blob = (struct cpbufinfo){
		.len = len,
		.eoff = 0,
		.flags = CPBI_F_SINGLE,
	};
	return cp_pack(pack, &i);
}

/** Pack *Blob* size to the generic packer.
 *
 * This is initial call for the packing blob in parts. You need to follow this
 * one or more calls to :c:macro:`cp_pack_blob_data` that needs to pack in total
 * number of bytes specified to this call.
 *
 * :param pack: Generic packer.
 * :param item: Item that needs to be used with subsequent
 *   :c:func:`cp_pack_blob_data` calls. The item doesn't have to be initialized.
 * :param siz: Number of bytes to be packed in total.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_blob_size(
	cp_pack_t pack, struct cpitem *item, size_t siz) {
	item->type = CPITEM_BLOB;
	item->as.Blob = (struct cpbufinfo){
		.len = 0,
		.eoff = siz,
		.flags = CPBI_F_FIRST,
	};
	return cp_pack(pack, item);
}

/** Pack *Blob* data to the generic packer.
 *
 * This is subsequent call after :c:func:`cp_pack_blob_size` and provides a way
 * to pack blobs in parts instead of all at once. The sum of all sizes passed to
 * this function must be the size initially passed to the
 * :c:func:`cp_pack_blob_size`.
 *
 * :param pack: Generic packer.
 * :param item: Item previously used with :c:macro:`cp_pack_blob_size`.
 * :param buf: Buffer to the bytes to be packed to the blob.
 * :param siz: Size of the **buf** (valid number of bytes to be packed).
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_blob_data(
	cp_pack_t pack, struct cpitem *item, const uint8_t *buf, size_t siz) {
	assert(item->type == CPITEM_BLOB);
	assert(item->as.Blob.eoff >= siz);
	if (siz == 0)
		return true;
	item->rbuf = buf;
	item->as.Blob.len = siz;
	item->as.Blob.eoff -= siz;
	item->as.Blob.flags &= ~CPBI_F_FIRST;
	if (item->as.Blob.eoff == 0)
		item->as.Blob.flags |= CPBI_F_LAST;
	return cp_pack(pack, item);
}

/** Pack *String* to the generic packer.
 *
 * :param pack: Generic packer.
 * :param buf: Buffer containing characters to be packed as a signle string.
 * :param len: Size of the **buf** (valid number of characters to be packed).
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_string(cp_pack_t pack, const char *buf, size_t len) {
	struct cpitem i;
	i.type = CPITEM_STRING;
	i.rchr = buf;
	i.as.String = (struct cpbufinfo){
		.len = len,
		.eoff = 0,
		.flags = CPBI_F_SINGLE,
	};
	return cp_pack(pack, &i);
}

/** Pack C-*String* to the generic packer.
 *
 * :pack pack: Generic packer.
 * :pack str: C-String (null terminated string) to be packed. It can be ``NULL``
 *   and in such case Null is packed instead.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull(1)]]
static inline bool cp_pack_str(cp_pack_t pack, const char *str) {
	return str ? cp_pack_string(pack, str, strlen(str)) : cp_pack_null(pack);
}

/** Pack *String* generated from format string to the generic packer.
 *
 * :param pack: Generic packer.
 * :param fmt: printf format string.
 * :param args: list of variadic arguments to be passed to the printf.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_vfstr(cp_pack_t pack, const char *fmt, va_list args) {
	va_list cargs;
	va_copy(cargs, args);
	int siz = vsnprintf(NULL, 0, fmt, cargs);
	va_end(cargs);
	char str[siz + 1];
	assert(vsnprintf(str, siz + 1, fmt, args) == siz);
	return cp_pack_string(pack, str, siz);
}

/** Pack *String* generated from format string to the generic packer.
 *
 * :param pack: Generic packer.
 * :param fmt: printf format string.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_fstr(cp_pack_t pack, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	size_t res = cp_pack_vfstr(pack, fmt, args);
	va_end(args);
	return res;
}

/** Pack *String* size to the generic packer.
 *
 * This is initial call for the packing string in parts. You need to follow this
 * with one or more calls to :c:func:`cp_pack_string_data` that needs to pack in
 * total number of bytes specified to this call.
 *
 * :param pack: Generic packer.
 * :param item: Item that needs to be used with subsequent
 *   :c:func:`cp_pack_string_data` calls. The item doesn't have to be
 *   initialized.
 * :param siz: Number of bytes to be packed in total.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_string_size(
	cp_pack_t pack, struct cpitem *item, size_t siz) {
	item->type = CPITEM_STRING;
	item->as.String = (struct cpbufinfo){
		.len = 0,
		.eoff = siz,
		.flags = CPBI_F_FIRST,
	};
	return cp_pack(pack, item);
}

/** Pack *String* data to the generic packer.
 *
 * This is subsequent call after :c:func:`cp_pack_string_size` and provides a
 * way to pack strings in parts instead of all at once. The sum of all sizes
 * passed to this function must be the size initially passed to the
 * :c:func:`cp_pack_string_size`.
 *
 * :param pack: Generic packer.
 * :param item: Item previously used with :c:func:`cp_pack_string_size`.
 * :param buf: Buffer to the bytes to be packed to the string.
 * :param siz: Size of the **buf** (valid number of bytes to be packed).
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_string_data(
	cp_pack_t pack, struct cpitem *item, const char *buf, size_t siz) {
	assert(item->type == CPITEM_STRING);
	assert(item->as.Blob.eoff >= siz);
	if (siz == 0)
		return true;
	item->rchr = buf;
	item->as.String.len = siz;
	item->as.String.eoff -= siz;
	item->as.Blob.flags &= ~CPBI_F_FIRST;
	if (item->as.String.eoff == 0)
		item->as.Blob.flags |= CPBI_F_LAST;
	return cp_pack(pack, item);
}

/** Pack *List* begining to the generic packer.
 *
 * You can follow this with items that are part of the list and then you need
 * to terminate the it with :c:func:`cp_pack_container_end`.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_list_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_LIST;
	return cp_pack(pack, &i);
}

/** Pack *Map* begining to the generic packer.
 *
 * You can follow this with always pair of items where first item is string key
 * and second item is value. After that you need to terminate *Map* with
 * :c:func:`cp_pack_container_end`.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_map_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_MAP;
	return cp_pack(pack, &i);
}

/** Pack *IMap* begining to the generic packer.
 *
 * You can follow this with always pair of items where first item is integer key
 * and second item is value. After that you need to terminate *IMap* with
 * :c:func:`cp_pack_container_end`.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_imap_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_IMAP;
	return cp_pack(pack, &i);
}

/** Pack *Meta* begining to the generic packer.
 *
 * You can follow this with always pair of items where first item is integer or
 * string key and second item is value. After that you need to terminate *Meta*
 * with :c:func:`cp_pack_container_end` and follow it with item that meta is
 * tied to.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_meta_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_META;
	return cp_pack(pack, &i);
}

/** Pack container end to the generic packer.
 *
 * This terminates containers openned with :c:func:`cp_pack_list_begin`,
 * :c:func:`cp_pack_map_begin`, :c:func:`cp_pack_imap_begin` and
 * :c:func:`cp_pack_meta_begin`.
 *
 * :param pack: Generic packer.
 * :return: Boolean signaling the pack success or failure.
 */
[[gnu::nonnull]]
static inline bool cp_pack_container_end(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_CONTAINER_END;
	return cp_pack(pack, &i);
}

/** Open :c:type:`FILE` stream that you can use for writing strings or blobs.
 *
 * This has overhead of establishing a :c:type:`FILE` object but on the other
 * hand it allows easy string and blob packing.
 *
 * Do not use **pack** for anything until you close the returned :c:type:`FILE`.
 *
 * :param pack: Generic packer.
 * :param str: Boolean deciding if you are writing characters or bytes (*String*
 *   or *Blob*)
 * :return: Instance of :c:type:`FILE`. It can return ``NULL`` when creation of
 *   :c:type:`FILE` stream fails.
 */
[[gnu::nonnull]]
FILE *cp_pack_fopen(cp_pack_t pack, bool str);

// clang-format off
#define __cp_pack_value(PACK, VALUE) \
	_Generic((VALUE), \
		bool: cp_pack_bool, \
		short: cp_pack_int, \
		int: cp_pack_int, \
		long: cp_pack_int, \
		long long: cp_pack_int, \
		unsigned short: cp_pack_uint, \
		unsigned: cp_pack_uint, \
		unsigned long: cp_pack_uint, \
		unsigned long long: cp_pack_uint, \
		float: cp_pack_double, \
		double: cp_pack_double, \
		long double: cp_pack_double, \
		struct cpdecimal: cp_pack_decimal, \
		struct cpdatetime: cp_pack_datetime, \
		char*: cp_pack_str, \
		const char*: cp_pack_str \
	)((PACK), (VALUE))
#define __cp_pack_value_l(PACK, VALUE, SIZ) \
	_Generic((VALUE), \
		char*: cp_pack_string, \
		const char*: cp_pack_string, \
		uint8_t*: cp_pack_blob, \
		const uint8_t*: cp_pack_blob \
	)((PACK), (VALUE), (SIZ))
// clang-format on
#define __cp_pack_value_select(_1, _2, X, ...) X
/** Pack value with generic packer with type detection.
 *
 * This allows you to be lazy and instead of expicitly typing the correct pack
 * function you can use this which selects the correct one based on the value
 * passed to it. The most of the C types are supported.
 *
 * This macro can be called with either just value or with value and size. The
 * simple example would be ``cp_pack_value(pack, 42)`` and ``cp_pack_value(pack,
 * buf, siz)``.
 */
#define cp_pack_value(PACK, ...) \
	__cp_pack_value_select(__VA_ARGS__, __cp_pack_value_l, __cp_pack_value)( \
		PACK, __VA_ARGS__)

#endif
