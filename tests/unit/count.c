/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#include <string.h>

#define SUITE "count"
#include <check_suite.h>

#include <foo.h>

TEST_CASE(simple) {}

TEST(simple, empty) {
	char *text = "";
	FILE *f = fmemopen(text, strlen(text), "r");
	ck_assert_int_eq(count_foo(f), 0);
	fclose(f);
}
END_TEST

TEST(simple, simple) {
	char *text = "foo: well\nups: nope\nfoo: fee\n";
	FILE *f = fmemopen(text, strlen(text), "r");
	ck_assert_int_eq(count_foo(f), 2);
	fclose(f);
}
END_TEST
