#include <shv/cp.h>

#define SUITE "cpitem"
#include <check_suite.h>


TEST_CASE(all) {}

static const struct {
	enum cpitem_type type;
	const char *name;
} type_str_d[] = {
	{CPITEM_INVALID, "INVALID"},
	{CPITEM_INT, "INT"},
	{-1, "INVALID"},
	{26341, "INVALID"},
};
ARRAY_TEST(all, type_str) {
	ck_assert_str_eq(_d.name, cpitem_type_str(_d.type));
}
END_TEST
