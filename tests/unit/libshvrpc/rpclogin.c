#include <shv/rpclogin.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpclogin"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

static const struct {
	struct rpclogin login;
	bool trusted;
	const char *cpon;
} packer_d[] = {
	{(struct rpclogin){
		 .username = "test",
		 .password = "test",
		 .login_type = RPC_LOGIN_PLAIN,
	 },
		true,
		"{\"login\":{\"user\":\"test\",\"password\":\"test\",\"type\":\"PLAIN\"}}"},
	{(struct rpclogin){
		 .username = "test",
		 .password = "test",
		 .login_type = RPC_LOGIN_PLAIN,
	 },
		false,
		"{\"login\":{\"user\":\"test\",\"password\":\"771ee79c4614cba556455d5fe0149e24ba0c56f5\",\"type\":\"SHA1\"}}"},
	{(struct rpclogin){
		 .username = "test",
		 .password = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
		 .login_type = RPC_LOGIN_SHA1,
	 },
		true,
		"{\"login\":{\"user\":\"test\",\"password\":\"771ee79c4614cba556455d5fe0149e24ba0c56f5\",\"type\":\"SHA1\"}}"},
	{(struct rpclogin){
		 .username = "test",
		 .password = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
		 .login_type = RPC_LOGIN_SHA1,
	 },
		false,
		"{\"login\":{\"user\":\"test\",\"password\":\"771ee79c4614cba556455d5fe0149e24ba0c56f5\",\"type\":\"SHA1\"}}"},
	{(struct rpclogin){
		 .device_id = "foo",
	 },
		true, "{\"options\":{\"device\":{\"deviceId\":\"foo\"}}}"},
	{(struct rpclogin){
		 .device_mountpoint = "test/foo",
	 },
		true, "{\"options\":{\"device\":{\"mountPoint\":\"test/foo\"}}}"},
	{(struct rpclogin){
		 .username = "test",
		 .login_type = RPC_LOGIN_PLAIN,
	 },
		true,
		"{\"login\":{\"user\":\"test\",\"password\":\"\",\"type\":\"PLAIN\"}}"},
	{(struct rpclogin){
		 .username = "test",
		 .login_type = RPC_LOGIN_PLAIN,
	 },
		false,
		"{\"login\":{\"user\":\"test\",\"password\":\"a7162d463b28666737f63034db39f03bca59b060\",\"type\":\"SHA1\"}}"},
};
ARRAY_TEST(pack, packer) {
	rpclogin_pack(packstream_pack, &_d.login, "nonce", _d.trusted);
	ck_assert_packstr(_d.cpon);
}


TEST_CASE(unpack) {}

static const struct {
	const char *cpon;
	struct rpclogin login;
} unpacker_d[] = {
	{"{\"login\":{\"user\":\"test\",\"password\":\"test\",\"type\":\"PLAIN\"}}",
		(struct rpclogin){
			.username = "test",
			.password = "test",
			.login_type = RPC_LOGIN_PLAIN,
		}},
	{"{\"login\":{\"user\":\"test\",\"password\":\"a7162d463b28666737f63034db39f03bca59b060\",\"type\":\"SHA1\",\"additional\":{}}}",
		(struct rpclogin){
			.username = "test",
			.password = "a7162d463b28666737f63034db39f03bca59b060",
			.login_type = RPC_LOGIN_SHA1,
		}},
	{"{\"options\":{\"device\":{\"deviceId\":\"foo\"}}}",
		(struct rpclogin){
			.device_id = "foo",
		}},
	{"{\"options\":{\"device\":{\"mountPoint\":\"test/foo\",\"unknown\":[1,2,3]},\"question\":42},\"any\":i{}}",
		(struct rpclogin){
			.device_mountpoint = "test/foo",
		}},
};
ARRAY_TEST(unpack, unpacker) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);
	struct obstack obstack;
	obstack_init(&obstack);

	struct rpclogin *login = rpclogin_unpack(unpack, &item, &obstack);
	ck_assert_ptr_nonnull(login);
	ck_assert_pstr_eq(login->username, _d.login.username);
	ck_assert_pstr_eq(login->password, _d.login.password);
	ck_assert_int_eq(login->login_type, _d.login.login_type);
	ck_assert_pstr_eq(login->device_id, _d.login.device_id);
	ck_assert_pstr_eq(login->device_mountpoint, _d.login.device_mountpoint);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}

static const char *const unpack_invalid_d[] = {
	"i{}",
	"[]",
	"42",
	"{\"login\":[]}",
	"{\"login\":{\"user\":null}}",
	"{\"login\":{\"password\":null}}",
	"{\"login\":{\"type\":null}}",
	"{\"login\":{\"type\":\"INVALID\"}}",
	"{\"options\":i{}}",
	"{\"options\":{\"device\":[]}}",
	"{\"options\":{\"device\":{\"deviceId\":[]}}}",
	"{\"options\":{\"device\":{\"mountPoint\":[]}}}",
};
ARRAY_TEST(unpack, unpack_invalid) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d);
	struct obstack obstack;
	obstack_init(&obstack);
	void *obase = obstack_base(&obstack);
	ck_assert_ptr_null(rpclogin_unpack(unpack, &item, &obstack));
	ck_assert_ptr_eq(obstack_base(&obstack), obase);
	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}


TEST_CASE(validate_password) {}

static const struct {
	struct rpclogin login;
	const char *password;
	enum rpclogin_type type;
	bool result;
} validation_d[] = {
	{(struct rpclogin){.password = "test", .login_type = RPC_LOGIN_PLAIN},
		"test", RPC_LOGIN_PLAIN, true},
	{(struct rpclogin){.password = "invalid", .login_type = RPC_LOGIN_PLAIN},
		"test", RPC_LOGIN_PLAIN, false},
	{(struct rpclogin){
		 .password = "771ee79c4614cba556455d5fe0149e24ba0c56f5",
		 .login_type = RPC_LOGIN_SHA1,
	 },
		"test", RPC_LOGIN_PLAIN, true},
	{(struct rpclogin){
		 .password = "81f344a7686a80b4c5293e8fdc0b0160c82c06a8",
		 .login_type = RPC_LOGIN_SHA1,
	 },
		"test", RPC_LOGIN_PLAIN, false},
	{(struct rpclogin){
		 .password = "771ee79c4614cba556455d5fe0149e24ba0c56f5",
		 .login_type = RPC_LOGIN_SHA1,
	 },
		"a94a8fe5ccb19ba61c4c0873d391e987982fbbd3", RPC_LOGIN_SHA1, true},
	{(struct rpclogin){.password = "test", .login_type = RPC_LOGIN_PLAIN},
		"a94a8fe5ccb19ba61c4c0873d391e987982fbbd3", RPC_LOGIN_SHA1, true},
	{(struct rpclogin){.password = "invalid", .login_type = RPC_LOGIN_PLAIN},
		"a94a8fe5ccb19ba61c4c0873d391e987982fbbd3", RPC_LOGIN_SHA1, false},
};
ARRAY_TEST(validate_password, validation) {
	ck_assert(rpclogin_validate_password(
				  &_d.login, _d.password, "nonce", _d.type) == _d.result);
}
