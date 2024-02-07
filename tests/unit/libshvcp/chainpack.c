#include <shv/cp.h>

#define SUITE "chainpack"
#include <check_suite.h>

#include "packstream.h"
#include "bdata.h"
#include "item.h"


TEST_CASE(pack, setup_packstream, teardown_packstream){};
TEST_CASE(unpack){};


static const struct {
	struct cpitem item;
	const struct bdata cp;
} single_d[] = {
	{{.type = CPITEM_NULL}, B(0x80)},
	{{.type = CPITEM_BOOL, .as.Bool = true}, B(0xfe)},
	{{.type = CPITEM_BOOL, .as.Bool = false}, B(0xfd)},
	{{.type = CPITEM_INT, .as.Int = 0}, B(0x40)},
	{{.type = CPITEM_INT, .as.Int = -1}, B(0x82, 0x41)},
	{{.type = CPITEM_INT, .as.Int = -2}, B(0x82, 0x42)},
	{{.type = CPITEM_INT, .as.Int = 7}, B(0x47)},
	{{.type = CPITEM_INT, .as.Int = 42}, B(0x6a)},
	{{.type = CPITEM_INT, .as.Int = 528}, B(0x82, 0x82, 0x10)},
	{{.type = CPITEM_INT, .as.Int = 2147483647},
		B(0x82, 0xf0, 0x7f, 0xff, 0xff, 0xff)},
	{{.type = CPITEM_INT, .as.Int = 4294967295},
		B(0x82, 0xf1, 0x00, 0xff, 0xff, 0xff, 0xff)},
	{{.type = CPITEM_INT, .as.Int = 9007199254740991},
		B(0x82, 0xf3, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff)},
	{{.type = CPITEM_INT, .as.Int = -9007199254740991},
		B(0x82, 0xf3, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff)},
	{{.type = CPITEM_UINT, .as.UInt = 0}, B(0x00)},
	{{.type = CPITEM_UINT, .as.UInt = 1}, B(0x01)},
	{{.type = CPITEM_UINT, .as.UInt = 126}, B(0x81, 0x7e)},
	{{.type = CPITEM_UINT, .as.UInt = 127}, B(0x81, 0x7f)},
	{{.type = CPITEM_UINT, .as.UInt = 2147483647},
		B(0x81, 0xf0, 0x7f, 0xff, 0xff, 0xff)},
	{{.type = CPITEM_UINT, .as.UInt = 4294967295},
		B(0x81, 0xf0, 0xff, 0xff, 0xff, 0xff)},
	{{.type = CPITEM_DOUBLE, .as.Double = 223.0},
		B(0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x6b, 0x40)},
	{{.type = CPITEM_DECIMAL, .as.Decimal = {.mantisa = 23, .exponent = -1}},
		B(0x8c, 0x17, 0x41)},
	{{.type = CPITEM_DATETIME, .as.Datetime = {.msecs = 1517529600001, .offutc = 0}},
		B(0x8d, 0x04)},
	{{.type = CPITEM_DATETIME, .as.Datetime = {.msecs = 1517529600001, .offutc = 60}},
		B(0x8d, 0x82, 0x11)},
	{{.type = CPITEM_STRING, .rchr = "", .as.String = {.flags = CPBI_F_SINGLE}},
		B(0x86, 0x00)},
	{{.type = CPITEM_STRING,
		 .rchr = "foo",
		 .as.String = {.len = 3, .flags = CPBI_F_SINGLE}},
		B(0x86, 0x03, 'f', 'o', 'o')},
	{{.type = CPITEM_STRING,
		 .rchr = "",
		 .as.String = {.flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		B(0x8e, 0x00)},
	{{.type = CPITEM_STRING,
		 .rchr = "foo",
		 .as.String = {.len = 3, .flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		B(0x8e, 'f', 'o', 'o', 0x00)},
	{{.type = CPITEM_BLOB,
		 .rbuf = (const uint8_t[]){0x61, 0x62, 0xcd, 0x0b, 0x0d, 0x0a},
		 .as.Blob = {.len = 6, .flags = CPBI_F_SINGLE}},
		B(0x85, 0x06, 0x61, 0x62, 0xcd, 0x0b, 0x0d, 0x0a)},
	{{.type = CPITEM_LIST}, B(0x88)},
	{{.type = CPITEM_MAP}, B(0x89)},
	{{.type = CPITEM_IMAP}, B(0x8a)},
	{{.type = CPITEM_META}, B(0x8b)},
	{{.type = CPITEM_CONTAINER_END}, B(0xff)},
};
ARRAY_TEST(pack, pack_single, single_d) {
	ck_assert_int_eq(chainpack_pack(packstream, &_d.item), _d.cp.len);
	ck_assert_packbuf(_d.cp.v, _d.cp.len);
}
END_TEST
ARRAY_TEST(unpack, unpack_single, single_d) {
	FILE *f = fmemopen((void *)_d.cp.v, _d.cp.len, "r");
	uint8_t buf[BUFSIZ];
	struct cpitem item = {.buf = buf, .bufsiz = BUFSIZ};
	ck_assert_int_eq(chainpack_unpack(f, &item), _d.cp.len);
	ck_assert_item(item, _d.item);
	fclose(f);
}
END_TEST


static const struct bdata skip_d[] = {
	B(0x86, 0x03, 'f', 'o', 'o'),					   /* String */
	B(0x8e, 'f', 'o', 'o', 0x00),					   /* CString */
	B(0x85, 0x06, 0x61, 0x62, 0xcd, 0x0b, 0x0d, 0x0a), /* Blob */
};
ARRAY_TEST(unpack, skip) {
	FILE *f = fmemopen((void *)_d.v, _d.len, "r");
	struct cpitem item = {.buf = NULL, .bufsiz = BUFSIZ};
	ck_assert_int_eq(chainpack_unpack(f, &item), _d.len);
	ck_assert_int_eq(fgetc(f), EOF);
	ck_assert(feof(f));
	fclose(f);
}
END_TEST

static struct bdata genericd = B(0x01, 0x02, 0xf8, 0xff);
TEST(pack, pack_raw) {
	struct cpitem item = {
		.type = CPITEM_RAW, .rbuf = genericd.v, .as.Blob.len = genericd.len};
	ck_assert_int_eq(chainpack_pack(packstream, &item), genericd.len);
	ck_assert_packbuf(genericd.v, genericd.len);
}
END_TEST
TEST(unpack, unpack_raw) {
	FILE *f = fmemopen((void *)genericd.v, genericd.len, "r");
	uint8_t buf[BUFSIZ];
	struct cpitem item = {.type = CPITEM_RAW, .buf = buf, .bufsiz = BUFSIZ};
	ck_assert_int_eq(chainpack_unpack(f, &item), genericd.len);
	ck_assert_int_eq(item.as.Blob.len, genericd.len);
	ck_assert_mem_eq(item.buf, genericd.v, genericd.len);
	fclose(f);
}
END_TEST

TEST(unpack, unpack_drop) {
	FILE *f = fmemopen((void *)genericd.v, genericd.len, "r");
	struct cpitem item = {.type = CPITEM_RAW, .buf = NULL, .bufsiz = BUFSIZ};
	ck_assert_int_eq(chainpack_unpack(f, &item), genericd.len);
	ck_assert(feof(f));
	ck_assert(!ferror(f));
	fclose(f);
}
END_TEST
