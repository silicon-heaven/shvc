#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shv/chainpack.h>

#define SUITE "chainpack"
#include <check_suite.h>
#include "packunpack.h"

#define UNPACK_NEXT \
	{ \
		chainpack_unpack_next(&unpack); \
		ck_assert_int_eq(unpack.err_no, CPCP_RC_OK); \
	} \
	while (false)

TEST_CASE(pack, setup_pack, teardown_pack){};
TEST_CASE(unpack){};


static const uint8_t cpnull[] = {0x80};
TEST(pack, pack_none) {
	chainpack_pack_null(&pack);
	ck_assert_stash(cpnull, sizeof(cpnull));
}
END_TEST
TEST(unpack, unpack_null) {
	cpcp_unpack_context_init(&unpack, cpnull, sizeof(cpnull), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_NULL);
}
END_TEST

static const struct {
	const bool v;
	const uint8_t cp;
} cpbool_d[] = {{true, 0xfe}, {false, 0xfd}};
ARRAY_TEST(pack, pack_bool, cpbool_d) {
	chainpack_pack_boolean(&pack, _d.v);
	ck_assert_uint_eq(pack.current - pack.start, 1);
	ck_assert_int_eq(stashbuf[0], _d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_bool, cpbool_d) {
	cpcp_unpack_context_init(&unpack, &_d.cp, 1, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_BOOLEAN);
	ck_assert(unpack.item.as.Bool == _d.v);
}
END_TEST

static const struct {
	const int64_t v;
	const struct bdata cp;
} cpint_d[] = {
	{0, B(0x40)},
	{-1, B(0x82, 0x41)},
	{-2, B(0x82, 0x42)},
	{7, B(0x47)},
	{42, B(0x6a)},
	{2147483647, B(0x82, 0xf0, 0x7f, 0xff, 0xff, 0xff)},
	{4294967295, B(0x82, 0xf1, 0x00, 0xff, 0xff, 0xff, 0xff)},
	{9007199254740991, B(0x82, 0xf3, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff)},
	{-9007199254740991, B(0x82, 0xf3, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff)},
};
ARRAY_TEST(pack, pack_int, cpint_d) {
	chainpack_pack_int(&pack, _d.v);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_int, cpint_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, _d.v);
}
END_TEST

static const struct {
	const uint64_t v;
	const struct bdata cp;
} cpuint_d[] = {
	{0, B(0x00)},
	{1, B(0x01)},
	{2147483647, B(0x81, 0xf0, 0x7f, 0xff, 0xff, 0xff)},
	{4294967295, B(0x81, 0xf0, 0xff, 0xff, 0xff, 0xff)},
};
ARRAY_TEST(pack, pack_uint, cpuint_d) {
	chainpack_pack_uint(&pack, _d.v);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_uint, cpuint_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_UINT);
	ck_assert_uint_eq(unpack.item.as.UInt, _d.v);
}
END_TEST

static const struct {
	const double v;
	const struct bdata cp;
} cpdouble_d[] = {
	{223.0, B(0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x6b, 0x40)},
};
ARRAY_TEST(pack, pack_double, cpdouble_d) {
	chainpack_pack_double(&pack, _d.v);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_double, cpdouble_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_DOUBLE);
	ck_assert_double_eq(unpack.item.as.Double, _d.v);
}
END_TEST

static const struct {
	const cpcp_decimal v;
	const struct bdata cp;
} cpdecimal_d[] = {
	{.v = {.mantisa = 23, .exponent = -1}, B(0x8c, 0x17, 0x41)},
};
ARRAY_TEST(pack, pack_decimal, cpdecimal_d) {
	chainpack_pack_decimal(&pack, &_d.v);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_decimal, cpdecimal_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_DECIMAL);
	ck_assert_int_eq(unpack.item.as.Decimal.mantisa, _d.v.mantisa);
	ck_assert_int_eq(unpack.item.as.Decimal.exponent, _d.v.exponent);
}
END_TEST

static const struct {
	const char *const v;
	const struct bdata cp;
} cpstring_d[] = {
	{.v = "", B(0x86, 0x00)},
	{.v = "foo", B(0x86, 0x03, 'f', 'o', 'o')},
};
ARRAY_TEST(pack, pack_string, cpstring_d) {
	chainpack_pack_string(&pack, _d.v, strlen(_d.v));
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_string, cpstring_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_STRING);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, strlen(_d.v));
	ck_assert_str_eq(unpack.item.as.String.chunk_start, _d.v);
}
END_TEST

static const struct {
	const char *const v;
	const struct bdata cp;
} cpcstring_d[] = {
	{.v = "", B(0x8e, 0x00)},
	{.v = "foo", B(0x8e, 'f', 'o', 'o', 0x00)},
};
ARRAY_TEST(pack, pack_cstring, cpcstring_d) {
	chainpack_pack_cstring_terminated(&pack, _d.v);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_cstring, cpcstring_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_STRING);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, strlen(_d.v));
	ck_assert_str_eq(unpack.item.as.String.chunk_start, _d.v);
}
END_TEST

static const struct {
	const struct bdata blob;
	const struct bdata cp;
} cpblob_d[] = {
	{B(0x61, 0x62, 0xcd, 0x0b, 0x0d, 0x0a),
		B(0x85, 0x06, 0x61, 0x62, 0xcd, 0x0b, 0x0d, 0x0a)},
};
ARRAY_TEST(pack, pack_blob, cpblob_d) {
	chainpack_pack_blob(&pack, _d.blob.v, _d.blob.len);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_blob, cpblob_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_BLOB);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, _d.blob.len);
	ck_assert_mem_eq(unpack.item.as.String.chunk_start, _d.blob.v, _d.blob.len);
}
END_TEST

static const struct {
	const cpcp_date_time v;
	const struct bdata cp;
} cpdatetime_d[] = {
	{.v = {.msecs_since_epoch = 1517529600001, .minutes_from_utc = 0},
		B(0x8d, 0x04)},
	{.v = {.msecs_since_epoch = 1517529600001, .minutes_from_utc = 60},
		B(0x8d, 0x82, 0x11)},
};
ARRAY_TEST(pack, pack_datetime, cpdatetime_d) {
	chainpack_pack_date_time(&pack, &_d.v);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_datetime, cpdatetime_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_DATE_TIME);
	ck_assert_int_eq(
		unpack.item.as.DateTime.msecs_since_epoch, _d.v.msecs_since_epoch);
	ck_assert_int_eq(
		unpack.item.as.DateTime.minutes_from_utc, _d.v.minutes_from_utc);
}
END_TEST

#define INTARRAY(...) \
	(int64_t[]){__VA_ARGS__}, sizeof((int64_t[]){__VA_ARGS__}) / sizeof(int64_t)
static const struct {
	const int64_t *const v;
	const size_t vcnt;
	const struct bdata cp;
} cplist_ints_d[] = {
	{INTARRAY(), B(0x88, 0xff)},
	{INTARRAY(1), B(0x88, 0x41, 0xff)},
	{INTARRAY(1, 2, 3), B(0x88, 0x41, 0x42, 0x43, 0xff)},
};
ARRAY_TEST(pack, pack_list_ints, cplist_ints_d) {
	chainpack_pack_list_begin(&pack);
	for (size_t i = 0; i < _d.vcnt; i++)
		chainpack_pack_int(&pack, _d.v[i]);
	chainpack_pack_container_end(&pack);
	ck_assert_stash(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_list_ints, cplist_ints_d) {
	cpcp_unpack_context_init(&unpack, _d.cp.v, _d.cp.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_LIST);
	for (size_t i = 0; i < _d.vcnt; i++) {
		UNPACK_NEXT
		ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
		ck_assert_int_eq(unpack.item.as.Int, _d.v[i]);
	}
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_CONTAINER_END);
}
END_TEST

const struct bdata list_in_list_d = B(0x88, 0x88, 0xff, 0xff);
TEST(pack, pack_list_in_list) {
	chainpack_pack_list_begin(&pack);
	chainpack_pack_list_begin(&pack);
	chainpack_pack_container_end(&pack);
	chainpack_pack_container_end(&pack);
	ck_assert_stash(list_in_list_d.v, list_in_list_d.len);
}
END_TEST
TEST(unpack, unpack_list_in_list) {
	cpcp_unpack_context_init(
		&unpack, list_in_list_d.v, list_in_list_d.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_LIST);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_LIST);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_CONTAINER_END);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_CONTAINER_END);
}
END_TEST

static const struct bdata map_d =
	B(0x89, 0x86, 0x03, 'f', 'o', 'o', 0x86, 0x03, 'b', 'a', 'r', 0xff);
TEST(pack, pack_map) {
	chainpack_pack_map_begin(&pack);
	chainpack_pack_string(&pack, "foo", 3);
	chainpack_pack_string(&pack, "bar", 3);
	chainpack_pack_container_end(&pack);
	ck_assert_stash(map_d.v, map_d.len);
}
END_TEST
TEST(unpack, unpack_map) {
	cpcp_unpack_context_init(&unpack, map_d.v, map_d.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_MAP);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_STRING);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, 3);
	ck_assert_mem_eq(unpack.item.as.String.chunk_start, "foo", 3);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_STRING);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, 3);
	ck_assert_mem_eq(unpack.item.as.String.chunk_start, "bar", 3);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_CONTAINER_END);
}
END_TEST

static const struct bdata imap_d = B(0x8a, 0x41, 0x42, 0xff);
TEST(pack, pack_imap) {
	chainpack_pack_imap_begin(&pack);
	chainpack_pack_int(&pack, 1);
	chainpack_pack_int(&pack, 2);
	chainpack_pack_container_end(&pack);
	ck_assert_stash(imap_d.v, imap_d.len);
}
END_TEST
TEST(unpack, unpack_imap) {
	cpcp_unpack_context_init(&unpack, imap_d.v, imap_d.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_IMAP);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, 1);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, 2);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_CONTAINER_END);
}
END_TEST

static const struct bdata meta_d = B(0x8b, 0x41, 0x42, 0xff, 0x43);
TEST(pack, pack_meta) {
	chainpack_pack_meta_begin(&pack);
	chainpack_pack_int(&pack, 1);
	chainpack_pack_int(&pack, 2);
	chainpack_pack_container_end(&pack);
	chainpack_pack_int(&pack, 3);
	ck_assert_stash(meta_d.v, meta_d.len);
}
END_TEST
TEST(unpack, unpack_meta) {
	cpcp_unpack_context_init(&unpack, meta_d.v, meta_d.len, NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_META);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, 1);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, 2);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_CONTAINER_END);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, 3);
}
END_TEST

/* Packing function should not generate any output when pack context is in error */
TEST(pack,pack_uint_error){
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_pack_uint(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_int_error){
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_pack_int(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_decimal_error){
	const static cpcp_decimal d={0,0};
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_pack_decimal(&pack,&d);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_double_error){
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_pack_double(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_date_error){
	static const cpcp_date_time time_er={0,0};
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_pack_date_time(&pack,&time_er);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
static const void (*pack_error_simple_call_d[])(cpcp_pack_context*) = {
	chainpack_pack_null,
	chainpack_pack_list_begin,
	chainpack_pack_map_begin,
	chainpack_pack_imap_begin,
	chainpack_pack_meta_begin,
	chainpack_pack_container_end
};
ARRAY_TEST(pack, pack_error_simple_call) {
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	_d(&pack);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_bool_error){
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_pack_boolean(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(unpack,unpack_next_error){
	cpcp_unpack_context_init(&unpack,NULL,0,NULL,NULL);
	unpack.err_no=CPCP_RC_LOGICAL_ERROR;
	chainpack_unpack_next(&unpack);
	ck_assert_ptr_eq(unpack.start,unpack.current);
}
END_TEST
TEST(unpack,unnpack_next_string_error){
	cpcp_unpack_context_init(&unpack,NULL,0,NULL,NULL);
	unpack.item.type=CPCP_ITEM_STRING;
	unpack.item.as.String.last_chunk=0;
	chainpack_unpack_next(&unpack);
	ck_assert_ptr_eq(unpack.start,unpack.current);
}
END_TEST
TEST(unpack,unpack_next_blob_error){
	cpcp_unpack_context_init(&unpack,NULL,0,NULL,NULL);
	unpack.item.type=CPCP_ITEM_BLOB;
	unpack.item.as.String.last_chunk=0;
	chainpack_unpack_next(&unpack);
	ck_assert_ptr_eq(unpack.start,unpack.current);
}
END_TEST
