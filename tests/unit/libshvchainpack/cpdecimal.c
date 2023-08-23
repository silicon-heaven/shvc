#include <shv/cp.h>

#define SUITE "cpdecimal"
#include <check_suite.h>


TEST_CASE(all) {}

static const struct {
	double f;
	struct cpdecimal d;
} _d[] = {
	{0., (struct cpdecimal){}},
	{1., (struct cpdecimal){.mantisa = 1}},
	{-1., (struct cpdecimal){.mantisa = -1}},
	{42.124, (struct cpdecimal){.mantisa = 42124, .exponent = -3}},
};

ARRAY_TEST(all, dectod, _d) {
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
