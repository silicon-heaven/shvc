#include <shv/chainpack.h>

#define SUITE "chainpackh"
#include <check_suite.h>


TEST_CASE(read){};
TEST_CASE(write){};


static const struct {
	uint8_t c;
	bool sign;
} scheme_signed_d[] = {
	{0x0, false},
	{0xff, true},
	{0x40, true},
	{0xbf, false},
};
ARRAY_TEST(read, scheme_signed) {
	ck_assert(chainpack_scheme_signed(_d.c) == _d.sign);
}
END_TEST

static const struct {
	uint8_t c;
	unsigned v;
} scheme_uint_d[] = {
	{0x0, 0},
	{0x3f, 0x3f},
	{0xff, 0x3f},
};
ARRAY_TEST(read, scheme_uint) {
	ck_assert_uint_eq(chainpack_scheme_uint(_d.c), _d.v);
}
END_TEST

static const struct {
	uint8_t c;
	bool is;
} is_int_d[] = {
	{0x00, false},
	{0xff, false},
	{0x7f, true},
	{0x3f, false},
	{CPS_Int, true},
};
ARRAY_TEST(read, is_int) {
	ck_assert(chainpack_is_int(_d.c) == _d.is);
}
END_TEST

static const struct {
	uint8_t c;
	bool is;
} is_uint_d[] = {
	{0x00, true},
	{0xff, false},
	{0x3f, true},
	{0x7f, false},
	{CPS_Int, true},
};
ARRAY_TEST(read, is_uint) {
	ck_assert(chainpack_is_uint(_d.c) == _d.is);
}
END_TEST

static const struct {
	uint8_t c;
	unsigned bytes;
} int_bytes_d[] = {
	{0x0f, 1},
	{0x7f, 1},
	{0xbf, 2},
	{0xdf, 3},
	{0xef, 4},
	{0xf0, 5},
	{0xf1, 6},
	{0xfd, 18},
};
ARRAY_TEST(read, int_bytes) {
	ck_assert_uint_eq(chainpack_int_bytes(_d.c), _d.bytes);
}
END_TEST

static const struct {
	uint8_t c;
	unsigned bytes;
	unsigned v;
} uint_value1_d[] = {
	{0x00, 1, 0},
	{0xff, 1, 0x7f},
	{0xff, 2, 0x3f},
	{0xff, 3, 0x1f},
	{0xff, 4, 0x0f},
	{0xff, 5, 0x00},
};
ARRAY_TEST(read, uint_value1) {
	ck_assert_uint_eq(chainpack_uint_value1(_d.c, _d.bytes), _d.v);
}
END_TEST

static const struct {
	uint8_t c;
	unsigned bytes;
	int v;
} int_value1_d[] = {
	{0x00, 1, 0},
	{0xff, 1, -0x3f},
	{0xbf, 1, 0x3f},
	{0xff, 2, -0x1f},
	{0xdf, 2, 0x1f},
	{0xff, 3, -0x0f},
	{0xef, 3, 0x0f},
	{0xff, 4, -0x07},
	{0xf7, 4, 0x07},
	{0xff, 5, 0x00},
};
ARRAY_TEST(read, int_value1) {
	ck_assert_int_eq(chainpack_int_value1(_d.c, _d.bytes), _d.v);
}
END_TEST

static const struct {
	unsigned long long v;
	unsigned bytes;
} w_uint_bytes_d[] = {
	{0, 1},
	{1, 1},
	{0x7f, 1},
	{0x8f, 2},
	{0x3fff, 2},
	{0x4fff, 3},
	{0x1fffff, 3},
	{0x2fffff, 4},
	{0xfffffff, 4},
	{0x1fffffff, 5},
	{0xffffffff, 5},
	{0x1ffffffff, 6},
	{0xffffffffff, 6},
	{0x1ffffffffff, 7},
};
ARRAY_TEST(write, w_uint_bytes) {
	ck_assert_uint_eq(chainpack_w_uint_bytes(_d.v), _d.bytes);
}
END_TEST

static const struct {
	long long v;
	unsigned bytes;
} w_int_bytes_d[] = {
	{0, 1},
	{1, 1},
	{-1, 1},
	{0x3f, 1},
	{-0x3f, 1},
	{0x4f, 2},
	{-0x4f, 2},
	{0x1fff, 2},
	{0x2fff, 3},
	{0x0fffff, 3},
	{0x7ffffff, 4},
	{0x8ffffff, 5},
	{0x7fffffff, 5},
	{0x8fffffff, 6},
	{0x7fffffffff, 6},
	{0x8fffffffff, 7},
};
ARRAY_TEST(write, w_int_bytes) {
	ck_assert_uint_eq(chainpack_w_int_bytes(_d.v), _d.bytes);
}
END_TEST

static const struct {
	unsigned long long num;
	unsigned bytes;
	uint8_t byte;
} w_uint_value1_d[] = {
	{0, 1, 0x00},
	{1, 1, 0x01},
	{0x7f, 1, 0x7f},
	{0x8e, 2, 0x80},
};
ARRAY_TEST(write, w_uint_value1) {
	ck_assert_uint_eq(chainpack_w_uint_value1(_d.num, _d.bytes), _d.byte);
}
END_TEST

static const struct {
	long long num;
	unsigned bytes;
	uint8_t byte;
} w_int_value1_d[] = {
	{0, 1, 0x00},
	{1, 1, 0x01},
	{-1, 1, 0x41},
};
ARRAY_TEST(write, w_int_value1) {
	ck_assert_uint_eq(chainpack_w_int_value1(_d.num, _d.bytes), _d.byte);
}
END_TEST
