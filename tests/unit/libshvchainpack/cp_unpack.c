#include <shv/cp_unpack.h>
#include <shv/chainpack.h>
#include <stdlib.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "cp_unpack"
#include <check_suite.h>
#include "unpack.h"
#include "item.h"


TEST_CASE(unpack){};


TEST(unpack, chainpack_unpack_null) {
	struct bdata b = B(CPS_Null);
	cp_unpack_t unpack = unpack_chainpack(&b);
	struct cpitem item = (struct cpitem){};
	ck_assert_uint_eq(cp_unpack(unpack, &item), 1);
	unpack_free(unpack);
}
END_TEST
TEST(unpack, cpon_unpack_null) {
	const char *str = "null";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	ck_assert_uint_eq(cp_unpack(unpack, &item), 4);
	unpack_free(unpack);
}
END_TEST

/* Test our internal ability to allocate more state contexts */
TEST(unpack, cpon_unpack_ctx_realloc) {
	const char *str = "[[[]]]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	for (int i = 0; i < 3; i++) {
		ck_assert_uint_eq(cp_unpack(unpack, &item), 1);
		ck_assert_int_eq(item.type, CPITEM_LIST);
	}
	for (int i = 0; i < 3; i++) {
		ck_assert_uint_eq(cp_unpack(unpack, &item), 1);
		ck_assert_int_eq(item.type, CPITEM_CONTAINER_END);
	}

	unpack_free(unpack);
}
END_TEST

TEST(unpack, drop_string) {
	const char *str = "\"Some string we want to skip\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_STRING);
	cp_unpack_drop(unpack, &item);
	ck_assert_item_type(item, CPITEM_STRING);
	ck_assert(item.as.String.flags & CPBI_F_LAST);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_INVALID);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, skip_int) {
	const char *str = "[1,2]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_LIST);
	cp_unpack_skip(unpack, &item);
	cp_unpack(unpack, &item);
	ck_assert_item_int(item, 2);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_CONTAINER_END);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, skip_string) {
	const char *str = "[\"Some string we want to skip\"]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_LIST);
	cp_unpack_skip(unpack, &item);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_CONTAINER_END);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, skip_string_fin) {
	const char *str = "[\"Some string we want to skip\",\"Some other\"]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_LIST);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_STRING);
	cp_unpack_skip(unpack, &item);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_CONTAINER_END);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, skip_containers) {
	const char *str = "[{0:<>null}]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_LIST);
	cp_unpack_skip(unpack, &item);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_CONTAINER_END);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, finish_containers) {
	const char *str = "[{0:<>[null]}]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_LIST);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_MAP);
	cp_unpack_finish(unpack, &item, 1);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_CONTAINER_END);

	unpack_free(unpack);
}
END_TEST


TEST(unpack, unpack_strdup) {
	const char *str = "\"Some text\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	char *res = cp_unpack_strdup(unpack, &item);
	ck_assert_str_eq(res, "Some text");
	free(res);
	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_strndup) {
	const char *str = "\"Some text\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	char *res = cp_unpack_strndup(unpack, &item, 5);
	ck_assert_str_eq(res, "Some ");
	free(res);
	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_memdup) {
	const char *str = "b\"123\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	uint8_t *res;
	size_t siz;
	cp_unpack_memdup(unpack, &item, &res, &siz);
	struct bdata exp = B('1', '2', '3');
	ck_assert_int_eq(siz, exp.len);
	ck_assert_mem_eq(res, exp.v, siz);
	free(res);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_memndup) {
	const char *str = "b\"1234567890\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	uint8_t *res;
	size_t siz = 7;
	cp_unpack_memndup(unpack, &item, &res, &siz);
	struct bdata exp = B('1', '2', '3', '4', '5', '6', '7');
	ck_assert_int_eq(siz, exp.len);
	ck_assert_mem_eq(res, exp.v, siz);
	free(res);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_strdupo) {
	const char *str = "\"Some text\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	struct obstack obstack;
	obstack_init(&obstack);
	char *res = cp_unpack_strdupo(unpack, &item, &obstack);
	ck_assert_str_eq(res, "Some text");
	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_strndupo) {
	const char *str = "\"Some text\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	struct obstack obstack;
	obstack_init(&obstack);
	char *res = cp_unpack_strndupo(unpack, &item, 5, &obstack);
	ck_assert_str_eq(res, "Some ");
	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_memdupo) {
	const char *str = "b\"123\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	struct obstack obstack;
	obstack_init(&obstack);

	uint8_t *res;
	size_t siz;
	cp_unpack_memdupo(unpack, &item, &res, &siz, &obstack);
	struct bdata exp = B('1', '2', '3');
	ck_assert_int_eq(siz, exp.len);
	ck_assert_mem_eq(res, exp.v, siz);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_memndupo) {
	const char *str = "b\"1234567890\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	struct obstack obstack;
	obstack_init(&obstack);

	uint8_t *res;
	size_t siz = 7;
	cp_unpack_memndupo(unpack, &item, &res, &siz, &obstack);
	struct bdata exp = B('1', '2', '3', '4', '5', '6', '7');
	ck_assert_int_eq(siz, exp.len);
	ck_assert_mem_eq(res, exp.v, siz);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_strdup_invalid) {
	const char *str = "0";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	ck_assert_ptr_null(cp_unpack_strdup(unpack, &item));
	ck_assert_int_eq(item.type, CPITEM_INT);
	unpack_free(unpack);
}
END_TEST


TEST(unpack, unpack_strncpy) {
	const char *str = "\"Some text\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	char buf[6];
	ck_assert_int_eq(cp_unpack_strncpy(unpack, &item, buf, 5), 5);
	buf[5] = '\0';
	ck_assert_str_eq(buf, "Some ");
	ck_assert_int_eq(cp_unpack_strncpy(unpack, &item, buf, 5), 4);
	ck_assert_str_eq(buf, "text");

	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_memcpy) {
	const char *str = "x\"010203040506070809\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	uint8_t buf[6];
	ck_assert_int_eq(cp_unpack_memcpy(unpack, &item, buf, 5), 5);
	uint8_t exp1[] = {0x1, 0x2, 0x3, 0x4, 0x5};
	ck_assert_mem_eq(buf, exp1, 5);
	ck_assert_int_eq(cp_unpack_memcpy(unpack, &item, buf, 5), 4);
	uint8_t exp2[] = {0x6, 0x7, 0x8, 0x9};
	ck_assert_mem_eq(buf, exp2, 4);

	unpack_free(unpack);
}
END_TEST


TEST(unpack, unpack_fopen_string) {
	const char *str = "\"Some: 42:string\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	FILE *f = cp_unpack_fopen(unpack, &item);
	ck_assert_item_type(item, CPITEM_STRING);
	int d;
	char buf[10];
	ck_assert_int_eq(fscanf(f, "Some: %d:%9s", &d, buf), 2);
	ck_assert_int_eq(d, 42);
	ck_assert_str_eq(buf, "string");
	fclose(f);

	unpack_free(unpack);
}
END_TEST

TEST(unpack, unpack_fopen_blob) {
	const char *str = "x\"010203040506070809\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};

	FILE *f = cp_unpack_fopen(unpack, &item);
	ck_assert_item_type(item, CPITEM_BLOB);
	char buf[10];
	ck_assert_int_eq(fread(buf, 1, 10, f), 9);
	uint8_t exp[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9};
	ck_assert_mem_eq(buf, exp, 9);
	fclose(f);

	unpack_free(unpack);
}
END_TEST
