#include <strset.h>
#include <stdlib.h>
#include <stdio.h>

#define SUITE "strset"
#include <check_suite.h>


TEST_CASE(all) {}


#define ck_assert_hash \
	do { \
		int hsh = 0; \
		for (size_t i = 0; i < set.cnt; i++) { \
			ck_assert_int_le(hsh, set.items[i].hash); \
			hsh = set.items[i].hash; \
		} \
	} while (false)


TEST(all, single) {
	struct strset set = {};

	shv_strset_add_const(&set, ".app");
	shv_strset_add_const(&set, ".app");
	shv_strset_add_const(&set, ".app");

	ck_assert_int_eq(set.cnt, 1);
	ck_assert_str_eq(set.items[0].str, ".app");
	shv_strset_free(&set);
}
END_TEST

TEST(all, four) {
	struct strset set = {};

	shv_strset_add_const(&set, ".app");
	shv_strset_add_const(&set, ".broker");
	shv_strset_add_const(&set, ".device");
	shv_strset_add_const(&set, "test");

	shv_strset_add_const(&set, ".app");
	shv_strset_add_const(&set, ".broker");
	shv_strset_add_const(&set, ".device");
	shv_strset_add_const(&set, "test");

	ck_assert_int_eq(set.cnt, 4);
	ck_assert_hash;

	shv_strset_free(&set);
}
END_TEST

TEST(all, five) {
	struct strset set = {};

	shv_strset_add_const(&set, "foo");
	shv_strset_add_const(&set, "fee");
	shv_strset_add_const(&set, "faa");
	shv_strset_add_const(&set, "bar");
	shv_strset_add_const(&set, "boooooooooooooooo");

	shv_strset_add_const(&set, "faa");
	shv_strset_add_const(&set, "bar");
	shv_strset_add_const(&set, "boooooooooooooooo");
	shv_strset_add_const(&set, "fee");
	shv_strset_add_const(&set, "foo");

	ck_assert_int_eq(set.cnt, 5);
	ck_assert_hash;

	shv_strset_free(&set);
}
END_TEST

TEST(all, alphabet) {
	struct strset set = {};

	for (int i = 0; i < 3; i++)
		for (char c = 'a'; c <= 'z'; c++) {
			char *str;
			ck_assert_int_ne(asprintf(&str, "char:%c", c), -1);
			shv_strset_add_dyn(&set, str);
			if (i > 0)
				free(str);
		}

	ck_assert_int_eq(set.cnt, 'z' - 'a' + 1);
	ck_assert_hash;

	shv_strset_free(&set);
}
END_TEST
