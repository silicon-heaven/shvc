#include <shv/rpcmsg.h>

#define SUITE "rpcmsg_pack"
#include <check_suite.h>
#include "packstream.h"


TEST_CASE(all, setup_packstream_pack_cpon, teardown_packstream_pack) {}

TEST(all, request) {
	rpcmsg_pack_request(packstream_pack, ".app", "echo", NULL, 42);
	ck_assert_packstr("<1:1,8:42,9:\".app\",10:\"echo\">i{1");
}
END_TEST

TEST(all, request_void) {
	rpcmsg_pack_request_void(packstream_pack, ".broker/app", "ping", "fanda", 42);
	ck_assert_packstr("<1:1,8:42,9:\".broker/app\",10:\"ping\",16:\"fanda\">i{}");
}
END_TEST

TEST(all, signal) {
	rpcmsg_pack_signal(
		packstream_pack, "value", "get", "qchng", NULL, RPCACCESS_READ, false);
	ck_assert_packstr("<1:1,9:\"value\",10:\"qchng\">i{1");
}
END_TEST

TEST(all, signal_void) {
	rpcmsg_pack_signal_void(packstream_pack, "node", "version", "chng", "foo",
		RPCACCESS_COMMAND, true);
	ck_assert_packstr(
		"<1:1,9:\"node\",10:\"chng\",19:\"version\",16:\"foo\",17:24,20:true>i{}");
}
END_TEST

TEST(all, resp) {
	struct rpcmsg_meta meta = {
		.request_id = 42,
		.cids = (long long[]){0, 3},
		.cids_cnt = 2,
	};
	rpcmsg_pack_response(packstream_pack, &meta);
	ck_assert_packstr("<1:1,8:42,11:[0,3]>i{2");
}
END_TEST

TEST(all, resp_void) {
	struct rpcmsg_meta meta = {
		.request_id = 42,
		.cids = (long long[]){3},
		.cids_cnt = 1,
	};
	rpcmsg_pack_response_void(packstream_pack, &meta);
	ck_assert_packstr("<1:1,8:42,11:3>i{}");
}
END_TEST

TEST(all, error) {
	struct rpcmsg_meta meta = {.request_id = 42};
	rpcmsg_pack_error(packstream_pack, &meta, RPCERR_UNKNOWN, "Our error");
	ck_assert_packstr("<1:1,8:42>i{3:i{1:9,2:\"Our error\"}}");
}
END_TEST

TEST(all, ferr) {
	struct rpcmsg_meta meta = {.request_id = 24};
	rpcmsg_pack_ferror(packstream_pack, &meta, RPCERR_USR1, "Fail of %d", 42);
	ck_assert_packstr("<1:1,8:24>i{3:i{1:6,2:\"Fail of 42\"}}");
}
END_TEST
