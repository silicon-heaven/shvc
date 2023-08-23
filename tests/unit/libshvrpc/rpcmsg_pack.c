#include <shv/rpcmsg.h>

#define SUITE "rpcmsg_pack"
#include <check_suite.h>
#include "packstream.h"


TEST_CASE(all, setup_packstream_pack_cpon, teardown_packstream_pack) {}

TEST(all, request) {
	rpcmsg_pack_request(packstream_pack, ".app", "ping", 42);
	ck_assert_packstr("<1:1,8:42,9:\".app\",10:\"ping\">i{1");
}
END_TEST

TEST(all, signal) {
	rpcmsg_pack_signal(packstream_pack, "value", "qchng");
	ck_assert_packstr("<1:1,9:\"value\",10:\"qchng\">i{1");
}
END_TEST

TEST(all, chng) {
	rpcmsg_pack_chng(packstream_pack, "value");
	ck_assert_packstr("<1:1,9:\"value\",10:\"chng\">i{1");
}
END_TEST

// TODO test pack response and error
