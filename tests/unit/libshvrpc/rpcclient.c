#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <shv/rpcclient_stream.h>
#include <shv/rpcmsg.h>
#include <shv/rpctransport.h>

#define SUITE "rpcclient"
#include <check_suite.h>

TEST_CASE(all) {}

TEST(all, peer_name_null) {
	struct rpcclient c = {};
	char buf[BUFSIZ];
	ck_assert_int_eq(rpcclient_peername(&c, buf, BUFSIZ), 0);
	ck_assert_str_eq(buf, "");
}
