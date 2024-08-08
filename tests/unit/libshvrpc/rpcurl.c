#include <stdlib.h>
#include <shv/rpcurl.h>

#define SUITE "rpcurl"
#include <check_suite.h>
#include "url.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free


TEST_CASE(parse){};


static const struct {
	const char *str;
	struct rpcurl url;
} parse_d[] = {
	{"unix:/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UNIX,
			.location = "/dev/null",
		}},
	{"unix:dir/socket",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UNIX,
			.location = "dir/socket",
		}},
	{"tcp://test@localhost:4242",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.login.username = "test",
			.port = 4242,
		}},
	{"tcp://localhost?user=test@example.com",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.login.username = "test@example.com",
			.port = 3755,
		}},
	{"tcp://localhost:4242?devid=foo&devmount=/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 4242,
			.login.device_id = "foo",
			.login.device_mountpoint = "/dev/null",
		}},
	{"tcp://localhost:4242?devid=foo&devmount=/dev/null&password=test",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 4242,
			.login.password = "test",
			.login.device_id = "foo",
			.login.device_mountpoint = "/dev/null",
		}},
	{"tcp://localhost:4242?devid=foo&devmount=/dev/null&shapass=xxxxxxxx",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 4242,
			.login.password = "xxxxxxxx",
			.login.login_type = RPC_LOGIN_SHA1,
			.login.device_id = "foo",
			.login.device_mountpoint = "/dev/null",
		}},
	{"tcp://[::]:4242",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "::",
			.port = 4242,
		}},
	{"serial:/dev/ttyUSB1",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TTY,
			.location = "/dev/ttyUSB1",
			.tty.baudrate = 115200,
		}},
	{"tty:/dev/ttyUSB1?baudrate=1152000",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TTY,
			.location = "/dev/ttyUSB1",
			.tty.baudrate = 1152000,
		}},
	{"", (struct rpcurl){.protocol = RPC_PROTOCOL_UNIX}},
	{"socket",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UNIX,
			.location = "socket",
		}},
	{"/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UNIX,
			.location = "/dev/null",
		}},
	{"unix:/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UNIX,
			.location = "/dev/null",
		}},
	{"serial:/dev/ttyUSB0",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TTY,
			.location = "/dev/ttyUSB0",
		}},
	{"tcp://localhost",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 3755,
		}},
	{"tcp://localhost?devid=foo",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 3755,
			.login.device_id = "foo",
		}},
};
ARRAY_TEST(parse, parse) {
	struct obstack obstack;
	obstack_init(&obstack);
	const char *err;
	struct rpcurl *url = rpcurl_parse(_d.str, &err, &obstack);
	ck_assert_ptr_nonnull(url);
	ck_assert_ptr_null(err);
	ck_assert_rpcurl_eq(url, &_d.url);
	obstack_free(&obstack, NULL);
}

static const struct {
	const char *const str;
	size_t error;
} parse_invalid_d[] = {
	{"foo://some", 0},
	{"tcp://some:none?password=foo", 15}, // We parse it backward so port end
	{"tcp://some?invalid=foo", 11},
};
ARRAY_TEST(parse, parse_invalid, parse_invalid_d) {
	struct obstack obstack;
	obstack_init(&obstack);
	const char *err;
	ck_assert_ptr_null(rpcurl_parse(_d.str, &err, &obstack));
	ck_assert_ptr_eq(err, _d.str + _d.error);
	obstack_free(&obstack, NULL);
}
END_TEST
