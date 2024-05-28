#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>

#define SUITE "rpcclient_links"
#include <check_suite.h>
#include "broker.h"


TEST_CASE(all, setup_shvbroker, teardown_shvbroker) {}


TEST(all, stream_tcp) {
	rpcclient_t c = rpcclient_stream_tcp_connect("localhost", shvbroker_tcp_port);
	ck_assert_ptr_nonnull(c);
	// TODO send some message
	rpcclient_destroy(c);
}
END_TEST

TEST(all, stream_unix) {
	rpcclient_t c = rpcclient_stream_unix_connect(shvbroker_unix_path);
	ck_assert_ptr_nonnull(c);
	// TODO send some message
	rpcclient_destroy(c);
}
END_TEST
