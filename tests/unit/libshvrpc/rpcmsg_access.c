#include "shv/cp.h"
#include <shv/rpcmsg.h>
#include <stdlib.h>

#define SUITE "rpcmsg_access"
#include <check_suite.h>
#include "unpack.h"
#include "item.h"


TEST_CASE(all) {}


static const struct {
	enum rpcmsg_access acc;
	const char *str;
} acc_str_d[] = {
	{RPCMSG_ACC_BROWSE, "bws"},
	{RPCMSG_ACC_READ, "rd"},
	{RPCMSG_ACC_WRITE, "wr"},
	{RPCMSG_ACC_COMMAND, "cmd"},
	{RPCMSG_ACC_CONFIG, "cfg"},
	{RPCMSG_ACC_SERVICE, "srv"},
	{RPCMSG_ACC_SUPER_SERVICE, "ssrv"},
	{RPCMSG_ACC_DEVEL, "dev"},
	{RPCMSG_ACC_ADMIN, "su"},
	{RPCMSG_ACC_INVALID, ""},
};
ARRAY_TEST(all, acc_str) {
	ck_assert_str_eq(rpcmsg_access_str(_d.acc), _d.str);
}
END_TEST

static const struct {
	const char *str;
	enum rpcmsg_access acc;
} acc_extract_d[] = {
	{"bws", RPCMSG_ACC_BROWSE},
	{"rd", RPCMSG_ACC_READ},
	{"wr", RPCMSG_ACC_WRITE},
	{"cmd", RPCMSG_ACC_COMMAND},
	{"cfg", RPCMSG_ACC_CONFIG},
	{"srv", RPCMSG_ACC_SERVICE},
	{"ssrv", RPCMSG_ACC_SUPER_SERVICE},
	{"dev", RPCMSG_ACC_DEVEL},
	{"su", RPCMSG_ACC_ADMIN},
	{"wr,other", RPCMSG_ACC_WRITE},
	{"Some,write,rd", RPCMSG_ACC_READ},
	{"", RPCMSG_ACC_INVALID},
	{"some,other", RPCMSG_ACC_INVALID},
};
ARRAY_TEST(all, acc_extract) {
	ck_assert_int_eq(rpcmsg_access_extract(_d.str), _d.acc);
}
END_TEST

ARRAY_TEST(all, acc_unpack, acc_extract_d) {
	char *str;
	ck_assert_int_ne(asprintf(&str, "\"%s\"", _d.str), -1);
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	ck_assert_int_eq(rpcmsg_access_unpack(unpack, &item), _d.acc);
	unpack_free(unpack);
	free(str);
}
END_TEST

TEST(all, acc_unpack_invalid) {
	const char *str = "42";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	ck_assert_int_eq(rpcmsg_access_unpack(unpack, &item), RPCMSG_ACC_INVALID);
	unpack_free(unpack);
}

TEST(all, acc_unpack_invalid_blob) {
	const char *str = "x\"00\"";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	ck_assert_int_eq(rpcmsg_access_unpack(unpack, &item), RPCMSG_ACC_INVALID);
	unpack_free(unpack);
}

/* It should always unpack the whole string */
TEST(all, acc_unpack_fully) {
	const char *str = "[\"bws,and,some,other\"]";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_LIST);
	ck_assert_int_eq(rpcmsg_access_unpack(unpack, &item), RPCMSG_ACC_BROWSE);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CPITEM_CONTAINER_END);
	unpack_free(unpack);
}
