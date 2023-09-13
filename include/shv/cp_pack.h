#ifndef SHV_CP_PACK_H
#define SHV_CP_PACK_H

#include <shv/cp.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

typedef size_t (*cp_pack_func_t)(void *ptr, const struct cpitem *item);
typedef cp_pack_func_t *cp_pack_t;


struct cp_pack_chainpack {
	cp_pack_func_t func;
	FILE *f;
};

cp_pack_t cp_pack_chainpack_init(struct cp_pack_chainpack *pack, FILE *f)
	__attribute__((nonnull));

struct cp_pack_cpon {
	cp_pack_func_t func;
	FILE *f;
	struct cpon_state state;
};

cp_pack_t cp_pack_cpon_init(struct cp_pack_cpon *pack, FILE *f,
	const char *indent) __attribute__((nonnull(1, 2)));


#define cp_pack(PACK, ITEM) \
	({ \
		cp_pack_t __pack = PACK; \
		(*__pack)(__pack, (ITEM)); \
	})


static inline size_t cp_pack_null(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_NULL;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_bool(cp_pack_t pack, bool v) {
	struct cpitem i;
	i.type = CPITEM_BOOL;
	i.as.Bool = v;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_int(cp_pack_t pack, long long v) {
	struct cpitem i;
	i.type = CPITEM_INT;
	i.as.Int = v;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_uint(cp_pack_t pack, unsigned long long v) {
	struct cpitem i;
	i.type = CPITEM_UINT;
	i.as.UInt = v;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_double(cp_pack_t pack, double v) {
	struct cpitem i;
	i.type = CPITEM_DOUBLE;
	i.as.Double = v;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_decimal(cp_pack_t pack, struct cpdecimal v) {
	struct cpitem i;
	i.type = CPITEM_DECIMAL;
	i.as.Decimal = v;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_datetime(cp_pack_t pack, struct cpdatetime v) {
	struct cpitem i;
	i.type = CPITEM_DATETIME;
	i.as.Datetime = v;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_blob(cp_pack_t pack, const uint8_t *buf, size_t len) {
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

static inline size_t cp_pack_blob_size(
	cp_pack_t pack, struct cpitem *item, size_t siz) {
	item->type = CPITEM_BLOB;
	item->as.Blob = (struct cpbufinfo){
		.len = 0,
		.eoff = siz,
		.flags = CPBI_F_FIRST,
	};
	return cp_pack(pack, item);
}

static inline size_t cp_pack_blob_data(
	cp_pack_t pack, struct cpitem *item, const uint8_t *buf, size_t siz) {
	assert(item->type == CPITEM_BLOB);
	assert(item->as.Blob.eoff >= siz);
	item->rbuf = buf;
	item->as.Blob.len = siz;
	item->as.Blob.eoff -= siz;
	item->as.Blob.flags &= ~CPBI_F_FIRST;
	if (item->as.Blob.eoff == 0)
		item->as.Blob.flags |= CPBI_F_LAST;
	return cp_pack(pack, item);
}

static inline size_t cp_pack_string(cp_pack_t pack, const char *buf, size_t len) {
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

static inline size_t cp_pack_str(cp_pack_t pack, const char *str) {
	return cp_pack_string(pack, str, strlen(str));
}

static inline size_t cp_pack_vfstr(cp_pack_t pack, const char *fmt, va_list args) {
	va_list cargs;
	va_copy(cargs, args);
	int siz = vsnprintf(NULL, 0, fmt, cargs);
	va_end(cargs);
	char str[siz];
	assert(vsnprintf(str, siz, fmt, args) == siz);
	return cp_pack_string(pack, str, siz - 1);
}

static inline size_t cp_pack_fstr(cp_pack_t pack, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	size_t res = cp_pack_vfstr(pack, fmt, args);
	va_end(args);
	return res;
}

static inline size_t cp_pack_string_size(
	cp_pack_t pack, struct cpitem *item, size_t siz) {
	item->type = CPITEM_STRING;
	item->as.String = (struct cpbufinfo){
		.len = 0,
		.eoff = siz,
		.flags = CPBI_F_FIRST,
	};
	return cp_pack(pack, item);
}

static inline size_t cp_pack_string_data(
	cp_pack_t pack, struct cpitem *item, const char *buf, size_t siz) {
	assert(item->type == CPITEM_STRING);
	assert(item->as.Blob.eoff >= siz);
	item->rchr = buf;
	item->as.String.len = siz;
	item->as.String.eoff -= siz;
	item->as.Blob.flags &= ~CPBI_F_FIRST;
	if (item->as.String.eoff == 0)
		item->as.Blob.flags |= CPBI_F_LAST;
	return cp_pack(pack, item);
}

static inline size_t cp_pack_list_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_LIST;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_map_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_MAP;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_imap_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_IMAP;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_meta_begin(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_META;
	return cp_pack(pack, &i);
}

static inline size_t cp_pack_container_end(cp_pack_t pack) {
	struct cpitem i;
	i.type = CPITEM_CONTAINER_END;
	return cp_pack(pack, &i);
}

FILE *cp_pack_fopen(cp_pack_t pack, bool str) __attribute__((nonnull));

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
#define cp_pack_value(PACK, ...) \
	__cp_pack_value_select(__VA_ARGS__, __cp_pack_value_l, __cp_pack_value)( \
		PACK, __VA_ARGS__)

#endif
