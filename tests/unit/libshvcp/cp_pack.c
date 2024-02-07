#include <shv/cp_pack.h>
#include <shv/chainpack.h>

#define SUITE "cp_pack"
#include <check_suite.h>
#include "packstream.h"
#include "bdata.h"

struct cp_pack_chainpack pack_chainpack;
struct cp_pack_cpon pack_cpon;
cp_pack_t pack;

static void setup_chainpack(void) {
	setup_packstream();
	pack = cp_pack_chainpack_init(&pack_chainpack, packstream);
}

static void setup_cpon(void) {
	setup_packstream();
	pack = cp_pack_cpon_init(&pack_cpon, packstream, NULL);
}


TEST_CASE(chainpack, setup_chainpack, teardown_packstream){};
TEST_CASE(cpon, setup_cpon, teardown_packstream){};


TEST(chainpack, chainpack_null) {
	cp_pack_null(pack);
	struct bdata b = B(CPS_Null);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_null) {
	cp_pack_null(pack);
	ck_assert_packstr("null");
}
END_TEST

static void pack_bool_true(void) {
	bool v = true;
	cp_pack_value(pack, v);
}
TEST(chainpack, chainpack_bool_true) {
	pack_bool_true();
	struct bdata b = B(CPS_TRUE);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_bool_true) {
	pack_bool_true();
	ck_assert_packstr("true");
}
END_TEST

static void pack_bool_false(void) {
	bool v = false;
	cp_pack_value(pack, v);
}
TEST(chainpack, chainpack_bool_false) {
	pack_bool_false();
	struct bdata b = B(CPS_FALSE);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_bool_false) {
	pack_bool_false();
	ck_assert_packstr("false");
}
END_TEST

static void pack_int(void) {
	cp_pack_value(pack, 42);
}
TEST(chainpack, chainpack_int) {
	pack_int();
	struct bdata b = B(0x6a);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_int) {
	pack_int();
	ck_assert_packstr("42");
}
END_TEST

static void pack_uint(void) {
	unsigned v = 42;
	cp_pack_value(pack, v);
}
TEST(chainpack, chainpack_uint) {
	pack_uint();
	struct bdata b = B(0x2a);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_uint) {
	pack_uint();
	ck_assert_packstr("42u");
}
END_TEST

static void pack_double(void) {
	cp_pack_value(pack, 42.2);
}
TEST(chainpack, chainpack_double) {
	pack_double();
	struct bdata b = B(0x83, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x19, 0x45, 0x40);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_double) {
	pack_double();
	ck_assert_packstr("42.2");
}
END_TEST

static void pack_decimal(void) {
	struct cpdecimal v = (struct cpdecimal){.mantisa = 422, .exponent = -1};
	cp_pack_value(pack, v);
}
TEST(chainpack, chainpack_decimal) {
	pack_decimal();
	struct bdata b = B(0x8c, 0x81, 0xa6, 0x41);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_decimal) {
	pack_decimal();
	ck_assert_packstr("42.2");
}
END_TEST

static void pack_datetime(void) {
	struct cpdatetime v = (struct cpdatetime){.msecs = 2346, .offutc = -60};
	cp_pack_value(pack, v);
}
TEST(chainpack, chainpack_datetime) {
	pack_datetime();
	struct bdata b = B(0x8d, 0xf3, 0x82, 0xc2, 0xa7, 0xa0, 0x0d, 0xaa, 0x0f);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_datetime) {
	pack_datetime();
	ck_assert_packstr("d\"1970-01-01T00:00:02.346-01:00\"");
}
END_TEST

static void pack_blob(void) {
	struct bdata v = B(0x42, 0x11);
	cp_pack_value(pack, v.v, v.len);
}
TEST(chainpack, chainpack_blob) {
	pack_blob();
	struct bdata b = B(0x85, 0x02, 0x42, 0x11);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_blob) {
	pack_blob();
	ck_assert_packstr("b\"B\\11\"");
}
END_TEST

static void pack_blob_seq(void) {
	struct bdata v = B(0x42, 0x11);
	struct cpitem item;
	cp_pack_blob_size(pack, &item, v.len);
	for (size_t i = 0; i < v.len; i++)
		cp_pack_blob_data(pack, &item, &v.v[i], 1);
}
TEST(chainpack, chainpack_blob_seq) {
	pack_blob_seq();
	struct bdata b = B(0x85, 0x02, 0x42, 0x11);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_blob_seq) {
	pack_blob_seq();
	ck_assert_packstr("b\"B\\11\"");
}
END_TEST

static void pack_str(void) {
	const char *str = "foo";
	cp_pack_value(pack, str);
}
TEST(chainpack, chainpack_str) {
	pack_str();
	struct bdata b = B(0x86, 0x03, 'f', 'o', 'o');
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_str) {
	pack_str();
	ck_assert_packstr("\"foo\"");
}
END_TEST

static void pack_string(void) {
	const char *str = "foo";
	cp_pack_value(pack, str, 3);
}
TEST(chainpack, chainpack_string) {
	pack_string();
	struct bdata b = B(0x86, 0x03, 'f', 'o', 'o');
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_string) {
	pack_string();
	ck_assert_packstr("\"foo\"");
}
END_TEST

static void pack_string_seq(void) {
	const char *str = "foo";
	struct cpitem item;
	cp_pack_string_size(pack, &item, 3);
	for (int i = 0; i < 3; i++)
		cp_pack_string_data(pack, &item, &str[i], 1);
}
TEST(chainpack, chainpack_string_seq) {
	pack_string_seq();
	struct bdata b = B(0x86, 0x03, 'f', 'o', 'o');
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_string_seq) {
	pack_string_seq();
	ck_assert_packstr("\"foo\"");
}
END_TEST

static void pack_list(void) {
	cp_pack_list_begin(pack);
	cp_pack_list_begin(pack);
	cp_pack_list_begin(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
}
TEST(chainpack, chainpack_list) {
	pack_list();
	struct bdata b = B(0x88, 0x88, 0x88, 0xff, 0xff, 0xff);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_list) {
	pack_list();
	ck_assert_packstr("[[[]]]");
}
END_TEST

static void pack_map(void) {
	cp_pack_map_begin(pack);
	cp_pack_map_begin(pack);
	cp_pack_map_begin(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
}
TEST(chainpack, chainpack_map) {
	pack_map();
	struct bdata b = B(0x89, 0x89, 0x89, 0xff, 0xff, 0xff);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_map) {
	pack_map();
	ck_assert_packstr("{{{}}}");
}
END_TEST

static void pack_imap(void) {
	cp_pack_imap_begin(pack);
	cp_pack_imap_begin(pack);
	cp_pack_imap_begin(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
}
TEST(chainpack, chainpack_imap) {
	pack_imap();
	struct bdata b = B(0x8a, 0x8a, 0x8a, 0xff, 0xff, 0xff);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_imap) {
	pack_imap();
	ck_assert_packstr("i{i{i{}}}");
}
END_TEST

static void pack_containers(void) {
	cp_pack_list_begin(pack);
	cp_pack_map_begin(pack);
	cp_pack_imap_begin(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
	cp_pack_container_end(pack);
}
TEST(chainpack, chainpack_containers) {
	pack_containers();
	struct bdata b = B(0x88, 0x89, 0x8a, 0xff, 0xff, 0xff);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_containers) {
	pack_containers();
	ck_assert_packstr("[{i{}}]");
}
END_TEST

static void pack_meta(void) {
	cp_pack_meta_begin(pack);
	cp_pack_int(pack, 0);
	cp_pack_meta_begin(pack);
	cp_pack_container_end(pack);
	cp_pack_int(pack, 0);
	cp_pack_container_end(pack);
	cp_pack_null(pack);
}
TEST(chainpack, chainpack_meta) {
	pack_meta();
	struct bdata b = B(0x8b, 0x40, 0x8b, 0xff, 0x40, 0xff, 0x80);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_meta) {
	pack_meta();
	ck_assert_packstr("<0:<>0>null");
}
END_TEST


static void pack_fopen_str(void) {
	FILE *f = cp_pack_fopen(pack, true);
	ck_assert_ptr_nonnull(f);
	fputs("Num", f);
	fprintf(f, ": %d", 42);
	fclose(f);
}
TEST(chainpack, chainpack_fopen_str) {
	pack_fopen_str();
	struct bdata b = B(0x8e, 'N', 'u', 'm', ':', ' ', '4', '2', 0x00);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_fopen_str) {
	pack_fopen_str();
	ck_assert_packstr("\"Num: 42\"");
}
END_TEST

static void pack_fopen_blob(void) {
	FILE *f = cp_pack_fopen(pack, false);
	ck_assert_ptr_nonnull(f);
	setvbuf(f, NULL, _IONBF, 0);
	struct bdata v = B(0x42, 0x11);
	for (size_t i = 0; i < v.len; i++)
		fputc(v.v[i], f);
	fclose(f);
}
TEST(chainpack, chainpack_fopen_blob) {
	pack_fopen_blob();
	struct bdata b = B(0x8f, 0x01, 0x42, 0x01, 0x11, 0x00);
	ck_assert_packbuf(b.v, b.len);
}
END_TEST
TEST(cpon, cpon_fopen_blob) {
	pack_fopen_blob();
	ck_assert_packstr("b\"B\\11\"");
}
END_TEST
