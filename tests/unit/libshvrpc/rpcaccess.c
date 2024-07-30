#include <stdlib.h>
#include <shv/rpcmsg.h>

#define SUITE "rpcaccess"
#include <check_suite.h>


TEST_CASE(all) {}


static const struct {
	rpcaccess_t acc;
	const char *str;
} acc_str_d[] = {
	{RPCACCESS_BROWSE, "bws"},
	{RPCACCESS_READ, "rd"},
	{RPCACCESS_WRITE, "wr"},
	{RPCACCESS_COMMAND, "cmd"},
	{RPCACCESS_CONFIG, "cfg"},
	{RPCACCESS_SERVICE, "srv"},
	{RPCACCESS_SUPER_SERVICE, "ssrv"},
	{RPCACCESS_DEVEL, "dev"},
	{RPCACCESS_ADMIN, "su"},
	{RPCACCESS_NONE, ""},
	{42, ""},
};
ARRAY_TEST(all, acc_str) {
	ck_assert_str_eq(rpcaccess_granted_str(_d.acc), _d.str);
}
END_TEST

static const struct {
	const char *str;
	rpcaccess_t acc;
	const char *rest;
} acc_extract_d[] = {
	{"bws", RPCACCESS_BROWSE, ""},
	{"rd", RPCACCESS_READ, ""},
	{"wr", RPCACCESS_WRITE, ""},
	{"cmd", RPCACCESS_COMMAND, ""},
	{"cfg", RPCACCESS_CONFIG, ""},
	{"srv", RPCACCESS_SERVICE, ""},
	{"ssrv", RPCACCESS_SUPER_SERVICE, ""},
	{"dev", RPCACCESS_DEVEL, ""},
	{"su", RPCACCESS_ADMIN, ""},
	{"wr,other", RPCACCESS_WRITE, "other"},
	{"other,wr", RPCACCESS_WRITE, "other"},
	{"Some,write,rd", RPCACCESS_READ, "Some,write"},
	{"Some,wr,read", RPCACCESS_WRITE, "Some,read"},
	{"su,some,admin", RPCACCESS_ADMIN, "some,admin"},
	{"", RPCACCESS_NONE, ""},
	{"some,other", RPCACCESS_NONE, "some,other"},
};
ARRAY_TEST(all, acc_extract) {
	char *str = strdup(_d.str);
	ck_assert_int_eq(rpcaccess_granted_extract(str), _d.acc);
	ck_assert_str_eq(str, _d.rest);
	free(str);
}
END_TEST
