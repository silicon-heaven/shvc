#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shv/cpon.h>

#define SUITE "cpon"
#include <check_suite.h>
#include "packunpack.h"

#define UNPACK_NEXT \
	{ \
		cpon_unpack_next(&unpack); \
		ck_assert_int_eq(unpack.err_no, CPCP_RC_OK); \
	} \
	while (false)

TEST_CASE(pack, setup_pack, teardown_pack){};
TEST_CASE(unpack){};


static const char *const cpnull = "null";
TEST(pack, pack_none) {
	cpon_pack_null(&pack);
	ck_assert_stashstr(cpnull);
}
END_TEST
TEST(unpack, unpack_null) {
	cpcp_unpack_context_init(&unpack, cpnull, strlen(cpnull), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_NULL);
}
END_TEST

static const struct {
	const bool v;
	const char *const cp;
} cpbool_d[] = {{true, "true"}, {false, "false"}};
ARRAY_TEST(pack, pack_bool, cpbool_d) {
	cpon_pack_boolean(&pack, _d.v);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_bool, cpbool_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_BOOLEAN);
	ck_assert(unpack.item.as.Bool == _d.v);
}
END_TEST

static const struct {
	const int64_t v;
	const char *const cp;
} cpint_d[] = {
	{0, "0"},
	{-1, "-1"},
	{-2, "-2"},
	{7, "7"},
	{42, "42"},
	{2147483647, "2147483647"},
	{4294967295, "4294967295"},
	{9007199254740991, "9007199254740991"},
	{-9007199254740991, "-9007199254740991"},
};
ARRAY_TEST(pack, pack_int, cpint_d) {
	cpon_pack_int(&pack, _d.v);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_int, cpint_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_INT);
	ck_assert_int_eq(unpack.item.as.Int, _d.v);
}
END_TEST

static const struct {
	const uint64_t v;
	const char *const cp;
} cpuint_d[] = {
	{0, "0u"},
	{1, "1u"},
	{2147483647, "2147483647u"},
	{4294967295, "4294967295u"},
};
ARRAY_TEST(pack, pack_uint, cpuint_d) {
	cpon_pack_uint(&pack, _d.v);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_uint, cpuint_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_UINT);
	ck_assert_int_eq(unpack.item.as.UInt, _d.v);
}
END_TEST

struct cpdecimal {
	const cpcp_decimal v;
	const char *const cp;
};
static const struct cpdecimal cpdecimal_d[] = {
	{.v = {}, "0."},
	{.v = {.mantisa = 223}, "223."},
	{.v = {.mantisa = 23, .exponent = -1}, "2.3"},
};
ARRAY_TEST(pack, pack_decimal, cpdecimal_d) {
	cpon_pack_decimal(&pack, &_d.v);
	ck_assert_stashstr(_d.cp);
}
END_TEST
static void test_unpack_decimal(struct cpdecimal _d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_DOUBLE);
	ck_assert_int_eq(unpack.item.as.Decimal.mantisa, _d.v.mantisa);
	ck_assert_int_eq(unpack.item.as.Decimal.exponent, _d.v.exponent);
}
ARRAY_TEST(unpack, unpack_decimal, cpdecimal_d) {
	test_unpack_decimal(_d);
}
END_TEST
static const struct cpdecimal cpdecimal_unpack_d[] = {
	// TODO it should be more like exponent 0 not -1 🤷
	{.v = {.exponent = -1}, "0.0"},
	{.v = {.mantisa = 2230, .exponent = -1}, "223.0"},
};
ARRAY_TEST(unpack, unpack_only_decimal, cpdecimal_unpack_d) {
	test_unpack_decimal(_d);
}
END_TEST

static const struct {
	const char *const v;
	const char *const cp;
} cpstring_d[] = {
	{"", "\"\""},
	{"foo", "\"foo\""},
	{"dvaačtyřicet", "\"dvaačtyřicet\""},
	{"some\t\"tab\"", "\"some\\t\\\"tab\\\"\""},
};
ARRAY_TEST(pack, pack_string, cpstring_d) {
	cpon_pack_string_terminated(&pack, _d.v);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_string, cpstring_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_UINT);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, strlen(_d.v));
	ck_assert_str_eq(unpack.item.as.String.chunk_start, _d.v);
}
END_TEST

static const struct {
	const struct bdata blob;
	const char *const cp;
} cpblob_d[] = {
	{B(0x61, 0x62, 0xcd, '\t', '\r', '\n'), "b\"ab\\cd\\t\\r\\n\""},
};
ARRAY_TEST(pack, pack_blob, cpblob_d) {
	cpon_pack_blob(&pack, _d.blob.v, _d.blob.len);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_blob, cpblob_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
	UNPACK_NEXT
	ck_assert_int_eq(unpack.item.type, CPCP_ITEM_BLOB);
	ck_assert_int_eq(unpack.item.as.String.chunk_size, _d.blob.len);
	ck_assert_mem_eq(unpack.item.as.String.chunk_start, _d.blob.v, _d.blob.len);
}
END_TEST

static const struct {
	const cpcp_date_time v;
	const char *const cp;
} cpdatetime_d[] = {
	{.v = {.msecs_since_epoch = 1517529600000, .minutes_from_utc = 0},
		"d\"2018-02-02T00:00:00Z\""},
	{.v = {.msecs_since_epoch = 1517529600001, .minutes_from_utc = 60},
		"d\"2018-02-02T01:00:00.001+01\""},
	{.v = {.msecs_since_epoch = 1809340212345, .minutes_from_utc = 60},
		"d\"2027-05-03T11:30:12.345+01\""},
};
ARRAY_TEST(pack, pack_datetime, cpdatetime_d) {
	cpon_pack_date_time(&pack, &_d.v);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_datetime, cpdatetime_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
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
	const char *const cp;
} cplist_ints_d[] = {
	{INTARRAY(), "[]"},
	{INTARRAY(1), "[1]"},
	{INTARRAY(1, 2, 3), "[1,2,3]"},
};
ARRAY_TEST(pack, pack_list_ints, cplist_ints_d) {
	cpon_pack_list_begin(&pack);
	for (size_t i = 0; i < _d.vcnt; i++) {
		cpon_pack_field_delim(&pack, i == 0, true);
		cpon_pack_int(&pack, _d.v[i]);
	}
	cpon_pack_list_end(&pack, true);
	ck_assert_stashstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_list_ints, cplist_ints_d) {
	cpcp_unpack_context_init(&unpack, _d.cp, strlen(_d.cp), NULL, NULL);
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

static const char *const list_in_list_d = "[[]]";
TEST(pack, pack_list_in_list) {
	cpon_pack_list_begin(&pack);
	cpon_pack_list_begin(&pack);
	cpon_pack_list_end(&pack, true);
	cpon_pack_list_end(&pack, true);
	ck_assert_stashstr(list_in_list_d);
}
END_TEST
TEST(unpack, unpack_list_in_list) {
	cpcp_unpack_context_init(
		&unpack, list_in_list_d, strlen(list_in_list_d), NULL, NULL);
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

static const char *const map_d = "{\"foo\":\"bar\"}";
TEST(pack, pack_map) {
	cpon_pack_map_begin(&pack);
	cpon_pack_string(&pack, "foo", 3);
	cpon_pack_key_val_delim(&pack);
	cpon_pack_string(&pack, "bar", 3);
	cpon_pack_map_end(&pack, true);
	ck_assert_stashstr(map_d);
}
END_TEST
TEST(unpack, unpack_map) {
	cpcp_unpack_context_init(&unpack, map_d, strlen(map_d), NULL, NULL);
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

static const char *const imap_d = "i{1:2}";
TEST(pack, pack_imap) {
	cpon_pack_imap_begin(&pack);
	cpon_pack_int(&pack, 1);
	cpon_pack_key_val_delim(&pack);
	cpon_pack_int(&pack, 2);
	cpon_pack_imap_end(&pack, true);
	ck_assert_stashstr(imap_d);
}
END_TEST
TEST(unpack, unpack_imap) {
	cpcp_unpack_context_init(&unpack, imap_d, strlen(imap_d), NULL, NULL);
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

static const char *const meta_d = "<1:2>3";
TEST(pack, pack_meta) {
	cpon_pack_meta_begin(&pack);
	cpon_pack_int(&pack, 1);
	cpon_pack_key_val_delim(&pack);
	cpon_pack_int(&pack, 2);
	cpon_pack_meta_end(&pack, true);
	cpon_pack_int(&pack, 3);
	ck_assert_stashstr(meta_d);
}
END_TEST
TEST(unpack, unpack_meta) {
	cpcp_unpack_context_init(&unpack, meta_d, strlen(meta_d), NULL, NULL);
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
	cpon_pack_uint(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_int_error){
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	cpon_pack_int(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
TEST(pack,pack_double_error){
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	cpon_pack_double(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
static const void (*pack_error_simple_call_d[])(cpcp_pack_context*) = {
	cpon_pack_null,
	cpon_pack_list_begin,
	cpon_pack_map_begin,
	cpon_pack_imap_begin,
	cpon_pack_meta_begin
};
ARRAY_TEST(pack, pack_error_simple_call) {
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	_d(&pack);
	ck_assert_ptr_eq(pack.start,pack.current);
}
END_TEST
static const void (*pack_error_bool_call_d[])(cpcp_pack_context*,bool) = {
	cpon_pack_boolean,
	cpon_pack_list_end,
	cpon_pack_map_end,
	cpon_pack_imap_end,
	cpon_pack_meta_end
};
ARRAY_TEST(pack, pack_error_bool_call) {
	pack.err_no=CPCP_RC_LOGICAL_ERROR;
	_d(&pack,0);
	ck_assert_ptr_eq(pack.start,pack.current);

}
END_TEST
TEST(unpack,unpack_insig_error){
	cpcp_unpack_context_init(&unpack,NULL,0,NULL,NULL);
	cpon_unpack_skip_insignificant(&unpack);
	ck_assert_ptr_eq(unpack.start,unpack.current);
}
END_TEST
TEST(unpack,unpack_next_error){
	cpcp_unpack_context_init(&unpack,NULL,0,NULL,NULL);
	unpack.err_no=CPCP_RC_LOGICAL_ERROR;
	cpon_unpack_next(&unpack);
	ck_assert_ptr_eq(unpack.start,unpack.current);
}
END_TEST

