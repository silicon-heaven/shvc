#ifndef SHV_CHAINPACK_H
#define SHV_CHAINPACK_H

#include <shv/cp.h>
#include <assert.h>
#include <string.h>


static inline ssize_t chainpack_pack_null(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_NULL;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_bool(FILE *f, bool v) {
	struct cpitem i;
	i.type = CP_ITEM_BOOL;
	i.as.Bool = v;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_int(FILE *f, int64_t v) {
	struct cpitem i;
	i.type = CP_ITEM_INT;
	i.as.Int = v;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_uint(FILE *f, uint64_t v) {
	struct cpitem i;
	i.type = CP_ITEM_UINT;
	i.as.UInt = v;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_double(FILE *f, double v) {
	struct cpitem i;
	i.type = CP_ITEM_DOUBLE;
	i.as.Double = v;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_decimal(FILE *f, struct cpdecimal v) {
	struct cpitem i;
	i.type = CP_ITEM_DECIMAL;
	i.as.Decimal = v;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_blob(FILE *f, const uint8_t *buf, size_t len) {
	struct cpitem i;
	i.type = CP_ITEM_BLOB;
	i.as.Blob = (struct cpbuf){
		.rbuf = buf,
		.len = len,
		.eoff = 0,
		.first = true,
		.last = true,
	};
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_blob_size(
	FILE *f, struct cpitem *item, size_t siz) {
	item->type = CP_ITEM_BLOB;
	item->as.Blob = (struct cpbuf){
		.buf = NULL,
		.len = 0,
		.eoff = siz,
		.first = true,
		.last = false,
	};
	return chainpack_pack(f, item);
}

static inline ssize_t chainpack_pack_blob_data(
	FILE *f, struct cpitem *item, const uint8_t *buf, size_t siz) {
	assert(item->type == CP_ITEM_BLOB);
	item->as.Blob.rbuf = buf;
	item->as.Blob.len = siz;
	item->as.Blob.eoff -= siz;
	item->as.Blob.first = false;
	item->as.Blob.last = item->as.Blob.eoff != 0;
	assert(item->as.Blob.eoff >= 0);
	return chainpack_pack(f, item);
}

static inline ssize_t chainpack_pack_string(FILE *f, const char *buf, size_t len) {
	struct cpitem i;
	i.type = CP_ITEM_STRING;
	i.as.String = (struct cpbuf){
		.rchr = buf,
		.len = len,
		.eoff = 0,
		.first = true,
		.last = true,
	};
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_string_size(
	FILE *f, struct cpitem *item, size_t siz) {
	item->type = CP_ITEM_STRING;
	item->as.String = (struct cpbuf){
		.rbuf = NULL,
		.len = 0,
		.eoff = siz,
	};
	return chainpack_pack(f, item);
}

static inline ssize_t chainpack_pack_string_data(
	FILE *f, struct cpitem *item, const char *buf, size_t siz) {
	assert(item->type == CP_ITEM_STRING);
	item->as.String.rchr = buf;
	item->as.String.len = siz;
	item->as.String.eoff -= siz;
	item->as.Blob.first = false;
	item->as.Blob.last = item->as.String.eoff != 0;
	assert(item->as.String.eoff >= 0);
	return chainpack_pack(f, item);
}

static inline ssize_t chainpack_pack_cstring(FILE *f, const char *str) {
	struct cpitem i;
	i.type = CP_ITEM_STRING;
	i.as.String = (struct cpbuf){
		.rchr = str,
		.len = strlen(str),
		.eoff = -1,
		.first = true,
		.last = true,
	};
	return chainpack_pack(f, &i);
}

/*! Pack the beginning of the C style string.
 *
 * Now you can send any text until the NULL byte. You can use `fprintf` and
 * other functions like you are used to. To end the string you need to call
 * `chainpack_pack_cstring_end`.
 */
static inline ssize_t chainpack_pack_cstring_begin(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_STRING;
	i.as.String = (struct cpbuf){
		.rchr = NULL,
		.len = 0,
		.eoff = -1,
		.first = true,
		.last = false,
	};
	return chainpack_pack(f, &i);
}

/*! End the C string style packing.
 *
 * This effectively writes NULL byte but it is preferred to call this functions
 * instead of just writing it.
 */
static inline ssize_t chainpack_pack_cstring_end(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_STRING;
	i.as.String = (struct cpbuf){
		.rchr = NULL,
		.len = 0,
		.eoff = -1,
		.first = false,
		.last = true,
	};
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_list_begin(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_LIST;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_map_begin(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_MAP;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_imap_begin(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_IMAP;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_meta_begin(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_META;
	return chainpack_pack(f, &i);
}

static inline ssize_t chainpack_pack_container_end(FILE *f) {
	struct cpitem i;
	i.type = CP_ITEM_CONTAINER_END;
	return chainpack_pack(f, &i);
}


// TODO chainpack_pack_value with type detection

#endif
