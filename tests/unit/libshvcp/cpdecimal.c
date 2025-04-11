#include <shv/cp.h>

#define SUITE "cpdecimal"
#include <check_suite.h>


TEST_CASE(all) {}

static const struct {
	struct cpdecimal val, norm;
} decnorm_d[] = {
	{{.mantissa = 1}, {.mantissa = 1}},
	{{.mantissa = 100}, {.mantissa = 1, .exponent = 2}},
	{{.mantissa = 100, .exponent = -7}, {.mantissa = 1, .exponent = -5}},
	{{}, {}},
	{{.exponent = 100}, {}},
};
ARRAY_TEST(all, decnorm) {
	struct cpdecimal d = _d.val;
	cpdecnorm(&d);
	ck_assert_int_eq(d.mantissa, _d.norm.mantissa);
	ck_assert_int_eq(d.exponent, _d.norm.exponent);
}
END_TEST

static const struct {
	double f;
	struct cpdecimal d;
} dec_d[] = {
	{0., (struct cpdecimal){}},
	{1., (struct cpdecimal){.mantissa = 1}},
	{-1., (struct cpdecimal){.mantissa = -1}},
	{42.124, (struct cpdecimal){.mantissa = 42124, .exponent = -3}},
	{400., (struct cpdecimal){.mantissa = 4, .exponent = 2}},
};

ARRAY_TEST(all, dectod, dec_d) {
	ck_assert_double_eq_tol(cpdectod(_d.d), _d.f, 0.0000001);
}
END_TEST

static const struct {
	struct cpdecimal in, out;
	int exponent;
} decexp_d[] = {
	{{42, 3}, {42, 3}, 3},
	{{42, 3}, {42000, 0}, 0},
	{{42, 3}, {420, 2}, 2},
	{{42, 3}, {4200000, -2}, -2},
	{{42, 3}, {4, 4}, 4},
	{{42, 3}, {0, 5}, 5},
	{{42, 3}, {0, 5}, 6},
};
ARRAY_TEST(all, decexp) {
	struct cpdecimal d = _d.in;
	ck_assert(cpdecexp(&d, _d.exponent));
	ck_assert_int_eq(d.mantissa, _d.out.mantissa);
	ck_assert_int_eq(d.exponent, _d.out.exponent);
}

static const struct {
	struct cpdecimal dec;
	int exponent;
} decexp_overflow_d[] = {
	{{42, 3}, -18},
};
ARRAY_TEST(all, decexp_overflow) {
	struct cpdecimal d = _d.dec;
	ck_assert(!cpdecexp(&d, _d.exponent));
}

static const struct {
	struct cpdecimal dec;
	int exp, val;
} cpdtoi_d[] = {
	{{42, 0}, -3, 42000},
	{{42, -2}, -3, 420},
	{{42, -4}, -3, 4},
};
ARRAY_TEST(all, cpdtoi) {
	int val;
	ck_assert(cpdtoi(_d.dec, _d.exp, val));
	ck_assert_int_eq(val, _d.val);
}

static const struct {
	int mantissa, exponent;
	struct cpdecimal dec;
} cpitod_d[] = {
	{42, -3, {42, -3}},
	{4200, -3, {42, -1}},
};
ARRAY_TEST(all, cpitod) {
	struct cpdecimal d = cpitod(_d.mantissa, _d.exponent);
	ck_assert_int_eq(d.mantissa, _d.dec.mantissa);
	ck_assert_int_eq(d.exponent, _d.dec.exponent);
}
