#include <shv/cp.h>

#define SUITE "cph"
#include <check_suite.h>


TEST_CASE(all) {}

static const struct {
	struct cpitem item;
	bool valid;
	int16_t value;
} extract_int16_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 42},
		.valid = true,
		.value = 42,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 42},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 32768},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = -32769},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 32767},
		.valid = true,
		.value = 32767,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = -32768},
		.valid = true,
		.value = -32768,
	},
};
ARRAY_TEST(all, extract_int16) {
	int16_t val;
	ck_assert(cpitem_extract_int(&_d.item, val) == _d.valid);
	if (_d.valid)
		ck_assert_int_eq(val, _d.value);
}


static const struct {
	struct cpitem item;
	bool valid;
	uint16_t value;
} extract_int16u_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 42},
		.valid = true,
		.value = 42,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = -42},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 0},
		.valid = true,
		.value = 0,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 65535},
		.valid = true,
		.value = 65535,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 65536},
		.valid = false,
	},
};
ARRAY_TEST(all, extract_int16u) {
	uint16_t val;
	ck_assert(cpitem_extract_int(&_d.item, val) == _d.valid);
	if (_d.valid)
		ck_assert_uint_eq(val, _d.value);
}

static const struct {
	struct cpitem item;
	bool valid;
	uint16_t value;
} extract_uint16_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 42},
		.valid = true,
		.value = 42,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 42},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = -1},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 65536},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 65535},
		.valid = true,
		.value = 65535,
	},
};
ARRAY_TEST(all, extract_uint16) {
	uint16_t val;
	ck_assert(cpitem_extract_uint(&_d.item, val) == _d.valid);
	if (_d.valid)
		ck_assert_uint_eq(val, _d.value);
}


static const struct {
	struct cpitem item;
	bool valid;
	int16_t value;
} extract_uint16i_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 42},
		.valid = true,
		.value = 42,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 0},
		.valid = true,
		.value = 0,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 0},
		.valid = true,
		.value = 0,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 32768},
		.valid = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 32767},
		.valid = true,
		.value = 32767,
	},
};
ARRAY_TEST(all, extract_uint16i) {
	int16_t val;
	ck_assert(cpitem_extract_uint(&_d.item, val) == _d.valid);
	if (_d.valid)
		ck_assert_int_eq(val, _d.value);
}


static const struct {
	struct cpitem item;
	uint64_t value;
} extract_uint64_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = UINT64_MAX},
		.value = UINT64_MAX,
	},
	{
		.item = (struct cpitem){.type = CPITEM_UINT, .as.UInt = 42},
		.value = 42,
	},
};
ARRAY_TEST(all, extract_uint64) {
	uint64_t val;
	ck_assert(cpitem_extract_uint(&_d.item, val));
	ck_assert_uint_eq(val, _d.value);
}


static const struct {
	struct cpitem item;
	bool valid;
	bool value;
} extract_bool_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_BOOL, .as.Bool = true},
		.valid = true,
		.value = true,
	},
	{
		.item = (struct cpitem){.type = CPITEM_BOOL, .as.Bool = false},
		.valid = true,
		.value = false,
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 42},
		.valid = false,
	},
};
ARRAY_TEST(all, extract_bool) {
	bool val;
	ck_assert(cpitem_extract_bool(&_d.item, val) == _d.valid);
	if (_d.valid)
		ck_assert(val == _d.value);
}


static const struct {
	struct cpitem item;
	bool valid;
	struct cpdatetime value;
} extract_datetime_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_DATETIME, .as.Datetime = {42, 60}},
		.valid = true,
		.value = {42, 60},
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 42},
		.valid = false,
	},
};
ARRAY_TEST(all, extract_datetime) {
	struct cpdatetime val;
	ck_assert(cpitem_extract_datetime(&_d.item, val) == _d.valid);
	if (_d.valid) {
		ck_assert_int_eq(val.msecs, _d.value.msecs);
		ck_assert_int_eq(val.offutc, _d.value.offutc);
	}
}

static const struct {
	struct cpitem item;
	bool valid;
	struct cpdecimal value;
} extract_decimal_d[] = {
	{
		.item = (struct cpitem){.type = CPITEM_DECIMAL, .as.Decimal = {7, 2}},
		.valid = true,
		.value = {7, 2},
	},
	{
		.item = (struct cpitem){.type = CPITEM_INT, .as.Int = 42},
		.valid = false,
	},
};
ARRAY_TEST(all, extract_decimal) {
	struct cpdecimal val;
	ck_assert(cpitem_extract_decimal(&_d.item, val) == _d.valid);
	if (_d.valid) {
		ck_assert_int_eq(val.mantisa, _d.value.mantisa);
		ck_assert_int_eq(val.exponent, _d.value.exponent);
	}
}
