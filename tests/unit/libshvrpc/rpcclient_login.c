#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include <stdlib.h>

#define SUITE "rpcclient_login"
#include <check_suite.h>
#include "broker.h"
#include "rpcclient_ping.h"


TEST_CASE(all, setup_shvbroker, teardown_shvbroker) {}

TEST(all, plain) {
	rpcclient_t c = rpcclient_stream_tcp_connect("localhost", shvbroker_tcp_port);
	ck_assert_ptr_nonnull(c);
	struct rpclogin_options opts = {
		.username = "admin",
		.password = "admin!123",
		.login_type = RPC_LOGIN_PLAIN,
	};
	ck_assert(rpcclient_login(c, &opts, NULL));

	rpcclient_ping_test(c);

	rpcclient_destroy(c);
}
END_TEST

TEST(all, sha1) {
	rpcclient_t c = rpcclient_stream_tcp_connect("localhost", shvbroker_tcp_port);
	ck_assert_ptr_nonnull(c);
	struct rpclogin_options opts = {
		.username = "admin",
		.password = "57a261a7bcb9e6cf1db80df501cdd89cee82957e",
		.login_type = RPC_LOGIN_SHA1,
	};
	ck_assert(rpcclient_login(c, &opts, NULL));

	rpcclient_ping_test(c);

	rpcclient_destroy(c);
}
END_TEST

TEST(all, invalid) {
	rpcclient_t c = rpcclient_stream_tcp_connect("localhost", shvbroker_tcp_port);
	ck_assert_ptr_nonnull(c);
	struct rpclogin_options opts = {
		.username = "admin",
		.password = "invalid",
		.login_type = RPC_LOGIN_PLAIN,
	};
	char *msg;
	ck_assert(!rpcclient_login(c, &opts, &msg));
	ck_assert_str_eq(msg, "Invalid login");

	rpcclient_destroy(c);
}
END_TEST
