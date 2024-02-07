#include <shv/cp.h>

#define SUITE "cpdecimal"
#include <check_suite.h>


TEST_CASE(all) {}

static const struct {
	struct cpdecimal val, norm;
} decnorm_d[] = {
	{{.mantisa = 1}, {.mantisa = 1}},
	{{.mantisa = 100}, {.mantisa = 1, .exponent = 2}},
	{{.mantisa = 100, .exponent = -7}, {.mantisa = 1, .exponent = -5}},
	{{}, {}},
	{{.exponent = 100}, {}},
};
ARRAY_TEST(all, decnorm) {
	struct cpdecimal d = _d.val;
	cpdecnorm(&d);
	ck_assert_int_eq(d.mantisa, _d.norm.mantisa);
	ck_assert_int_eq(d.exponent, _d.norm.exponent);
}
END_TEST

static const struct {
	double f;
	struct cpdecimal d;
} dec_d[] = {
	{0., (struct cpdecimal){}},
	{1., (struct cpdecimal){.mantisa = 1}},
	{-1., (struct cpdecimal){.mantisa = -1}},
	{42.124, (struct cpdecimal){.mantisa = 42124, .exponent = -3}},
};

ARRAY_TEST(all, dectod, dec_d) {
	ck_assert_double_eq_tol(cpdectod(_d.d), _d.f, 0.0000001);
}
END_TEST

#if 0
ARRAY_TEST(all, dtodec, _d) {
	struct cpdecimal d = cpdtodec(_d.f);
	ck_assert_int_eq(d.mantisa, _d.d.mantisa);
	ck_assert_int_eq(d.exponent, _d.d.exponent);
}
END_TEST
#endif
