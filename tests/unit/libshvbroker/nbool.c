#include "nbool.h"

#define SUITE "nbool"
#include <check_suite.h>

TEST_CASE(nbool) {}

TEST(nbool, null) {
	nbool_t v = NULL;
	ck_assert(!nbool(v, 0));
	ck_assert(!nbool(v, 42));
	ck_assert(!nbool(v, UINT_MAX));
}

TEST(nbool, generic) {
	nbool_t v = NULL;
	nbool_clear(&v, 0);
	ck_assert_ptr_null(v);
	nbool_set(&v, 0);
	ck_assert_ptr_nonnull(v);
	ck_assert_int_eq(v->cnt, 1);
	ck_assert(nbool(v, 0));
	ck_assert(!nbool(v, 1));
	ck_assert(!nbool(v, 42));
	nbool_set(&v, 42);
	ck_assert_ptr_nonnull(v);
	ck_assert_int_eq(v->cnt, 42 / NBOOL_N + 1);
	ck_assert(!nbool(v, 41));
	ck_assert(nbool(v, 42));
	ck_assert(!nbool(v, 43));
	nbool_clear(&v, 42);
	ck_assert_ptr_nonnull(v);
	ck_assert_int_eq(v->cnt, 1);
	nbool_clear(&v, 0);
	ck_assert_ptr_null(v);
}

TEST(nbool, or) {
	nbool_t v = NULL;
	nbool_t o = NULL;
	nbool_set(&o, 8);
	nbool_set(&o, 42);
	nbool_or(&v, o);
	ck_assert_int_eq(v->cnt, o->cnt);
	ck_assert(nbool(v, 8));
	ck_assert(nbool(v, 42));
	ck_assert(!nbool(v, 1));
	nbool_set(&o, 1);
	nbool_or(&v, o);
	ck_assert(nbool(v, 1));
	free(o);
	free(v);
}
