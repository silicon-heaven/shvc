#include <shv/rpcurl.h>

#define SUITE "rpcurl"
#include <check_suite.h>


TEST_CASE(parse){};


struct urld {
	const char *const str;
	struct rpcurl url;
};

static struct urld url_d[] = {
	{"localsocket:/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_LOCAL_SOCKET,
			.location = "/dev/null",
		}},
	{"localsocket:dir/socket",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_LOCAL_SOCKET,
			.location = "dir/socket",
		}},
	{"tcp://test@localhost:4242",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.username = "test",
			.port = 4242,
		}},
	{"udp://test@localhost:4242",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UDP,
			.location = "localhost",
			.username = "test",
			.port = 4242,
		}},
	{"tcp://localhost:4242?devid=foo&devmount=/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 4242,
			.device_id = "foo",
			.device_mountpoint = "/dev/null",
		}},
	{"tcp://localhost:4242?devid=foo&devmount=/dev/null&password=test",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 4242,
			.password = "test",
			.device_id = "foo",
			.device_mountpoint = "/dev/null",
		}},
	{"tcp://localhost:4242?devid=foo&devmount=/dev/null&shapass=xxxxxxxx",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 4242,
			.password = "xxxxxxxx",
			.login_type = RPC_LOGIN_SHA1,
			.device_id = "foo",
			.device_mountpoint = "/dev/null",
		}},
	{"tcp://[::]:4242",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "::",
			.port = 4242,
		}},
};

static struct urld parse_d[] = {
	{"", (struct rpcurl){.protocol = RPC_PROTOCOL_LOCAL_SOCKET}},
	{"socket",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_LOCAL_SOCKET,
			.location = "socket",
		}},
	{"/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_LOCAL_SOCKET,
			.location = "/dev/null",
		}},
	{"localsocket:/dev/null",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_LOCAL_SOCKET,
			.location = "/dev/null",
		}},
	{"serialport:/dev/ttyUSB0",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_SERIAL_PORT,
			.location = "/dev/ttyUSB0",
		}},
	{"tcp://localhost",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 3755,
		}},
	{"udp://localhost",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_UDP,
			.location = "localhost",
			.port = 3755,
		}},
	{"tcp://localhost?devid=foo",
		(struct rpcurl){
			.protocol = RPC_PROTOCOL_TCP,
			.location = "localhost",
			.port = 3755,
			.device_id = "foo",
		}},
};
static void parse_test(struct urld _d) {
	const char *err;
	struct rpcurl *url = rpcurl_parse(_d.str, &err);
	ck_assert_ptr_nonnull(url);
	ck_assert_ptr_null(err);
	ck_assert_int_eq(url->protocol, _d.url.protocol);
	ck_assert_pstr_eq(url->location, _d.url.location);
	ck_assert_int_eq(url->port, _d.url.port);
	ck_assert_pstr_eq(url->username, _d.url.username);
	ck_assert_pstr_eq(url->password, _d.url.password);
	ck_assert_int_eq(url->login_type, _d.url.login_type);
	ck_assert_pstr_eq(url->device_id, _d.url.device_id);
	ck_assert_pstr_eq(url->device_mountpoint, _d.url.device_mountpoint);
	rpcurl_free(url);
}
ARRAY_TEST(parse, parse_url, url_d) {
	parse_test(_d);
}
END_TEST
ARRAY_TEST(parse, parse_parse, parse_d) {
	parse_test(_d);
}
END_TEST

static const struct {
	const char *const str;
	size_t error;
} parse_invalid_d[] = {
	{"foo://some", 0},
	{"udp://some:none?password=foo", 15}, // We parse it backward so end of the
										  // port
	{"udp://some?invalid=foo", 11},
};
ARRAY_TEST(parse, parse_invalid, parse_invalid_d) {
	const char *err;
	ck_assert_ptr_null(rpcurl_parse(_d.str, &err));
	ck_assert_ptr_eq(err, _d.str + _d.error);
}
END_TEST
