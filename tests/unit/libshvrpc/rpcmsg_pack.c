#include <shv/rpcmsg.h>

#define SUITE "rpcmsg_pack"
#include <check_suite.h>
#include "packstream.h"


TEST_CASE(all, setup_packstream_pack_cpon, teardown_packstream_pack) {}

TEST(all, request) {
	rpcmsg_pack_request(packstream_pack, ".app", "echo", 42);
	ck_assert_packstr("<1:1,8:42,9:\".app\",10:\"echo\">i{1");
}
END_TEST

TEST(all, request_void) {
	rpcmsg_pack_request_void(packstream_pack, ".broker/app", "ping", 42);
	ck_assert_packstr("<1:1,8:42,9:\".broker/app\",10:\"ping\">i{}");
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

TEST(all, resp) {
	struct rpcmsg_meta meta = {
		.request_id = 42,
		.cids = {.ptr = (void *)(const uint8_t[]){0x88, 0x40, 0x43, 0xff}, .siz = 4},
	};
	rpcmsg_pack_response(packstream_pack, &meta);
	ck_assert_packstr("<1:1,8:42,11:[0,3]>i{2");
}
END_TEST

TEST(all, error) {
	struct rpcmsg_meta meta = {.request_id = 42};
	rpcmsg_pack_error(packstream_pack, &meta, RPCMSG_E_UNKNOWN, "Our error");
	ck_assert_packstr("<1:1,8:42>i{3:i{1:9,2:\"Our error\"}}");
}
END_TEST

TEST(all, ferr) {
	struct rpcmsg_meta meta = {.request_id = 24};
	rpcmsg_pack_ferror(
		packstream_pack, &meta, RPCMSG_E_INTERNAL_ERR, "Fail of %d", 42);
	ck_assert_packstr("<1:1,8:24>i{3:i{1:4,2:\"Fail of 42\"}}");
}
END_TEST
