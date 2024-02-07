#include "check.h"
#include <shv/cp.h>

#define SUITE "cpon"
#include <check_suite.h>

#include "packstream.h"
#include "item.h"
#include "bdata.h"


TEST_CASE(pack, setup_packstream, teardown_packstream){};
TEST_CASE(unpack){};


static const struct {
	struct cpitem item;
	const char *cp;
} single_d[] = {
	{{.type = CPITEM_NULL}, "null"},
	{{.type = CPITEM_BOOL, .as.Bool = true}, "true"},
	{{.type = CPITEM_BOOL, .as.Bool = false}, "false"},
	{{.type = CPITEM_INT, .as.Int = 0}, "0"},
	{{.type = CPITEM_INT, .as.Int = -1}, "-1"},
	{{.type = CPITEM_INT, .as.Int = -2}, "-2"},
	{{.type = CPITEM_INT, .as.Int = 7}, "7"},
	{{.type = CPITEM_INT, .as.Int = 42}, "42"},
	{{.type = CPITEM_INT, .as.Int = 2147483647}, "2147483647"},
	{{.type = CPITEM_INT, .as.Int = 4294967295}, "4294967295"},
	{{.type = CPITEM_INT, .as.Int = 9007199254740991}, "9007199254740991"},
	{{.type = CPITEM_INT, .as.Int = -9007199254740991}, "-9007199254740991"},
	{{.type = CPITEM_UINT, .as.UInt = 0}, "0u"},
	{{.type = CPITEM_UINT, .as.UInt = 1}, "1u"},
	{{.type = CPITEM_UINT, .as.UInt = 127}, "127u"},
	{{.type = CPITEM_UINT, .as.UInt = 2147483647}, "2147483647u"},
	{{.type = CPITEM_UINT, .as.UInt = 4294967295}, "4294967295u"},
	{{.type = CPITEM_DECIMAL, .as.Decimal = (struct cpdecimal){}}, "0."},
	{{.type = CPITEM_DECIMAL, .as.Decimal = (struct cpdecimal){.mantisa = 223}},
		"223."},
	{{.type = CPITEM_DECIMAL,
		 .as.Decimal = (struct cpdecimal){.mantisa = 223, .exponent = -1}},
		"22.3"},
	{{.type = CPITEM_DECIMAL,
		 .as.Decimal = (struct cpdecimal){.mantisa = 223, .exponent = 3}},
		"223000."},
	{{.type = CPITEM_DECIMAL,
		 .as.Decimal = (struct cpdecimal){.mantisa = 223, .exponent = -5}},
		"0.00223"},
	{{.type = CPITEM_DECIMAL,
		 .as.Decimal = (struct cpdecimal){.mantisa = 223, .exponent = 7}},
		"223e7"},
	{{.type = CPITEM_DECIMAL,
		 .as.Decimal = (struct cpdecimal){.mantisa = 223, .exponent = -12}},
		"223e-12"},
	{{.type = CPITEM_DATETIME, .as.Datetime = (struct cpdatetime){}},
		"d\"1970-01-01T00:00:00.000Z\""},
	{{.type = CPITEM_DATETIME, .as.Datetime = {.msecs = 1517529600001, .offutc = 0}},
		"d\"2018-02-02T00:00:00.001Z\""},
	{{.type = CPITEM_DATETIME, .as.Datetime = {.msecs = 1517529600001, .offutc = 60}},
		"d\"2018-02-02T00:00:00.001+01:00\""},
	{{.type = CPITEM_DATETIME,
		 .as.Datetime = {.msecs = 1692776567123, .offutc = -75}},
		"d\"2023-08-23T07:42:47.123-01:15\""},
	{{.type = CPITEM_STRING,
		 .rchr = "",
		 .as.String = {.flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"\"\""},
	{{.type = CPITEM_STRING,
		 .rchr = "foo",
		 .as.String = {.len = 3, .flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"\"foo\""},
	{{.type = CPITEM_STRING,
		 .rchr = "foo",
		 .as.String = {.len = 3, .flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"\"foo\""},
	{{.type = CPITEM_STRING,
		 .rchr = "\f\a\b\t",
		 .as.String = {.len = 4, .flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"\"\\f\\a\\b\\t\""},
	{{.type = CPITEM_BLOB,
		 .rbuf = (const uint8_t[]){},
		 .as.String = {.flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"b\"\""},
	{{.type = CPITEM_BLOB,
		 .rbuf = (const uint8_t[]){0x61, 0x62, 0xcd, 0x0b, 0x0d, 0x0a},
		 .as.String = {.len = 6, .flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"b\"ab\\CD\\v\\r\\n\""},
	{{.type = CPITEM_BLOB,
		 .rbuf = (const uint8_t[]){},
		 .as.String = {.flags = CPBI_F_SINGLE | CPBI_F_STREAM}},
		"b\"\""},
	{{.type = CPITEM_BLOB,
		 .rbuf = (const uint8_t[]){0xfe, 0x00, 0x42},
		 .as.String = {.len = 3, .flags = CPBI_F_SINGLE | CPBI_F_STREAM | CPBI_F_HEX}},
		"x\"FE0042\""},
};
ARRAY_TEST(pack, pack_single, single_d) {
	struct cpon_state st = {};
	/*ck_assert_int_eq(cpon_pack(packstream, &st, &_d.item), strlen(_d.cp));*/
	cpon_pack(packstream, &st, &_d.item);
	ck_assert_packstr(_d.cp);
}
END_TEST
ARRAY_TEST(unpack, unpack_single, single_d) {
	size_t len = strlen(_d.cp);
	FILE *f = fmemopen((void *)_d.cp, len, "r");
	struct cpon_state st = {};
	uint8_t buf[BUFSIZ];
	struct cpitem item = {.buf = buf, .bufsiz = BUFSIZ};
	ck_assert_int_eq(cpon_unpack(f, &st, &item), len);
	ck_assert_item(item, _d.item);
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
