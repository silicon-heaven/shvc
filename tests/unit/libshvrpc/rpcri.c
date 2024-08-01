#include <shv/rpcri.h>

#define SUITE "rpcri"
#include <check_suite.h>


TEST_CASE(match){};


struct rid {
	const char *const resource;
	const char *path;
	const char *method;
	const char *signal;
	const bool match;
};

static struct rid ri_d[] = {
	{
		"**:*",
		.path = ".app",
		.method = "name",
		.signal = NULL,
		true,
	},
	{
		"**:*",
		.path = "sub/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"**:*",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"**:get",
		.path = ".app",
		.method = "name",
		.signal = NULL,
		false,
	},
	{
		"**:get",
		.path = "sub/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"**:get",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"test/**:get",
		.path = ".app",
		.method = "name",
		.signal = NULL,
		false,
	},
	{
		"test/**:get",
		.path = "sub/device/track",
		.method = "get",
		.signal = NULL,
		false,
	},
	{
		"test/**:get",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"**:*:*",
		.path = ".app",
		.method = "name",
		.signal = NULL,
		false,
	},
	{
		"**:*:*",
		.path = "sub/device/track",
		.method = "get",
		.signal = NULL,
		false,
	},
	{
		"**:*:*",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		false,
	},
	{
		"**:*:*",
		.path = "test/device/track",
		.method = "get",
		.signal = "chng",
		true,
	},
	{
		"**:*:*",
		.path = "test/device/track",
		.method = "get",
		.signal = "mod",
		true,
	},
	{
		"**:*:*",
		.path = "test/device/track",
		.method = "ls",
		.signal = "lsmod",
		true,
	},
	{
		"**:get:*",
		.path = "test/device/track",
		.method = "get",
		.signal = "chng",
		true,
	},
	{
		"**:get:*",
		.path = "test/device/track",
		.method = "get",
		.signal = "mod",
		true,
	},
	{
		"**:get:*",
		.path = "test/device/track",
		.method = "ls",
		.signal = "lsmod",
		false,
	},
	{
		"test/**:get:*chng",
		.path = "test/device/track",
		.method = "get",
		.signal = "chng",
		true,
	},
	{
		"test/**:get:*chng",
		.path = "test/device/track",
		.method = "get",
		.signal = "mod",
		false,
	},
	{
		"test/**:get:*chng",
		.path = "test/device/track",
		.method = "ls",
		.signal = "lsmod",
		false,
	},
	{
		"test/*:ls:lsmod",
		.path = "test/device/track",
		.method = "get",
		.signal = "chng",
		false,
	},
	{
		"test/*:ls:lsmod",
		.path = "test/device/track",
		.method = "get",
		.signal = "mod",
		false,
	},
	{
		"test/*:ls:lsmod",
		.path = "test/device/track",
		.method = "ls",
		.signal = "lsmod",
		false,
	},
	{
		"test/**:get",
		.path = "test/device/track",
		.method = "get",
		.signal = "chng",
		true,
	},
	{
		"test/**:get",
		.path = "test/device/track",
		.method = "get",
		.signal = "mod",
		true,
	},
	{
		"test/**:get",
		.path = "test/device/track",
		.method = "ls",
		.signal = "lsmod",
		false,
	},
	{
		"[t]est/**:ge[t]",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"[a-z]est/**:ge[a-z]",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"[a-z]est/**:ge[a-z",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"[a-s]est/**:get",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		false,
	},
	{
		"test/**:get[a-z]",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		false,
	},
	{
		"test/**:get[!c]",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
	{
		"test/**:get[!c][a-z]",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		false,
	},
	{
		"test/**:get[!c",
		.path = "test/device/track",
		.method = "get",
		.signal = NULL,
		true,
	},
};

static void match_test(struct rid _d) {
	bool match = rpcri_match(_d.resource, _d.path, _d.method, _d.signal);
	ck_assert_int_eq(match, _d.match);
}
ARRAY_TEST(match, match_ri, ri_d) {
	match_test(_d);
}
END_TEST
