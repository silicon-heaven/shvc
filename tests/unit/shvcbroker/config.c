#include <stdlib.h>
#include <string.h>
#include <obstack.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "config"
#include <check_suite.h>
#include "tmpdir.h"
#include "url.h"

#include <config.h>

#define ck_assert_strarr_eq(a, b) \
	do { \
		if (b) { \
			ck_assert_ptr_nonnull(a); \
			size_t __i = 0; \
			do { \
				ck_assert_pstr_eq(a[__i], b[__i]); \
			} while (b[__i++]); \
		} else \
			ck_assert_ptr_null(a); \
	} while (false)


/* Test various command line arguments */
TEST_CASE(load, setup_tmpdir, teardown_tmpdir) {}

static const struct {
	const char *cpon;
	const struct config conf;
} config_d[] = {
	{"{}", (struct config){}},
	{"{\"name\":\"myname\"}", (struct config){.name = "myname"}},
	{"{\"listen\":[]}", (struct config){.listen_cnt = 0}},
	{"{\"listen\":[\"tcp://localhost:5345\",\"unix:/dev/ttyACM0\"]}",
		(struct config){
			.listen =
				(struct rpcurl *[]){
					&(struct rpcurl){
						.protocol = RPC_PROTOCOL_TCP,
						.location = "localhost",
						.port = 5345,
					},
					&(struct rpcurl){
						.protocol = RPC_PROTOCOL_UNIX,
						.location = "/dev/ttyACM0",
						.port = 0,
					},
				},
			.listen_cnt = 2,
		}},
	{"{\"roles\":{\"admin\":{}}}",
		(struct config){
			.roles =
				(struct role[]){
					{.name = "admin"},
				},
			.roles_cnt = 1,
		}},
	{"{\"users\":{\"admin\":{\"password\":\"admin!123\",\"role\":\"admin\"}},\"roles\":{\"admin\":{}}}",
		(struct config){
			.users = (struct user[]){{
				.name = "admin",
				.password = "admin!123",
				.login_type = RPC_LOGIN_PLAIN,
				.role = "admin",
			}},
			.users_cnt = 1,
			.roles = (struct role[]){{.name = "admin"}},
			.roles_cnt = 1,
		}},
	{"{\"users\":{\"test\":{\"sha1pass\":\"xxxxxxxx\",\"role\":\"test\"}},\"roles\":{\"browse\":{},\"test\":{}}}",
		(struct config){
			.users = (struct user[]){{
				.name = "test",
				.password = "xxxxxxxx",
				.login_type = RPC_LOGIN_SHA1,
				.role = "test",
			}},
			.users_cnt = 1,
			.roles = (struct role[]){{.name = "browse"}, {.name = "test"}},
			.roles_cnt = 2,
		}},
	{"{\"roles\":{\"test\":{\"mountPoints\":\"test/*\",\"access\":{\"wr\":\"test/**:*\",\"bws\":[\"**:ls\",\"**:dir\"]}}}}",
		(struct config){
			.roles = (struct role[]){{
				.name = "test",
				.ri_access[RPCACCESS_BROWSE] = (char *[]){"**:ls", "**:dir", NULL},
				.ri_access[RPCACCESS_WRITE] = (char *[]){"test/**:*", NULL},
				.ri_mount_points = (char *[]){"test/*", NULL},
			}},
			.roles_cnt = 1,
		}},
	{"{\"autosetups\":[{\"deviceId\":\"history\",\"role\":\"admin\",\"mountPoint\":\".history\",\"subscriptions\":\"**:*:*\"},{\"deviceId\":\"*\",\"role\":\"test\",\"mountPoint\":\"test/%d%i\"}]}",
		(struct config){
			.autosetups =
				(struct autosetup[]){
					{
						.device_ids = (char *[]){"history", NULL},
						.roles = (char *[]){"admin", NULL},
						.mount_point = ".history",
						.subscriptions = (char *[]){"**:*:*", NULL},
					},
					{
						.device_ids = (char *[]){"*", NULL},
						.roles = (char *[]){"test", NULL},
						.mount_point = "test/%d%i",
					},
				},
			.autosetups_cnt = 2,
		}},
};
ARRAY_TEST(load, config) {
	struct obstack obstack;
	obstack_init(&obstack);
	char *fpath = tmpdir_file("shvcbroker.cpon", _d.cpon);

	struct config *conf = config_load(fpath, &obstack);
	ck_assert_ptr_nonnull(conf);
	ck_assert_pstr_eq(conf->name, _d.conf.name);
	ck_assert_int_eq(conf->listen_cnt, _d.conf.listen_cnt);
	for (size_t i = 0; i < _d.conf.listen_cnt; i++)
		ck_assert_rpcurl_eq(conf->listen[i], _d.conf.listen[i]);
	ck_assert_int_eq(conf->roles_cnt, _d.conf.roles_cnt);
	for (size_t i = 0; i < _d.conf.roles_cnt; i++) {
		ck_assert_str_eq(conf->roles[i].name, _d.conf.roles[i].name);
		for (rpcaccess_t access = 0; access <= RPCACCESS_ADMIN; access++)
			ck_assert_strarr_eq(conf->roles[i].ri_access[access],
				_d.conf.roles[i].ri_access[access]);
		ck_assert_strarr_eq(
			conf->roles[i].ri_mount_points, _d.conf.roles[i].ri_mount_points);
	}
	ck_assert_int_eq(conf->users_cnt, _d.conf.users_cnt);
	for (size_t i = 0; i < _d.conf.users_cnt; i++) {
		ck_assert_str_eq(conf->users[i].name, _d.conf.users[i].name);
		ck_assert_pstr_eq(conf->users[i].password, _d.conf.users[i].password);
		ck_assert_int_eq(conf->users[i].login_type, _d.conf.users[i].login_type);
		ck_assert_pstr_eq(conf->users[i].role, _d.conf.users[i].role);
	}
	for (size_t i = 0; i < _d.conf.autosetups_cnt; i++) {
		// TODO
	}

	free(fpath);
	obstack_free(&obstack, NULL);
}
END_TEST

static const struct {
	const char *cpon;
	const char *error;
} invalid_config_d[] = {
	{"42", "Config: Must be Map\n"},
	{"[]", "Config: Must be Map\n"},
	{"{\"name\":42}", "Config.name: Must be String\n"},
	{"{\"invalid\":[]}", "Config.invalid: Not expected\n"},
	{"{\"listen\":[42]}", "Config.listen[0]: Must be String\n"},
	{"{\"listen\":[\"tty:/dev/ttyACM0\",\"invalid:foo\"]}",
		"Config.listen[1]: Invalid RPC URL format\n"},
	{"{\"listen\":42}", "Config.listen: Must be List\n"},
	{"{\"users\":[]}", "Config.users: Must be Map\n"},
	{"{\"users\":{\"test\":[]}}", "Config.users.test: Must be Map\n"},
	{"{\"users\":{\"test\":{\"sha1pass\":42}}",
		"Config.users.test.sha1pass: Must be String\n"},
	{"{\"users\":{\"test\":{\"role\":42}}}",
		"Config.users.test.role: Must be String\n"},
	{"{\"roles\":[]}", "Config.roles: Must be Map\n"},
	{"{\"roles\":{\"test\":[]}}", "Config.roles.test: Must be Map\n"},
	{"{\"roles\":{\"test\":{\"access\":[}}}",
		"Config.roles.test.access: Must be Map\n"},
	{"{\"roles\":{\"test\":{\"access\":{\"inval\":\"**:*\"}}}",
		"Config.roles.test.access.inval: Invalid access level\n"},
	{"{\"roles\":{\"test\":{\"access\":{\"wr\":[42]}}}",
		"Config.roles.test.access.wr[0]: Must be String\n"},
	{"{\"roles\":{\"test\":{\"mountPoints\":42}}",
		"Config.roles.test.mountPoints: Must be String or List of strings\n"},
	{"{\"autosetups\":{}}", "Config.autosetups: Must be List\n"},
	{"{\"autosetups\":[[]]", "Config.autosetups[0]: Must be Map\n"},
	{"{\"autosetups\":[{\"deviceId\":42}]",
		"Config.autosetups[0].deviceId: Must be String or List of strings\n"},
	{"{\"autosetups\":[{\"role\":42}]",
		"Config.autosetups[0].role: Must be String or List of strings\n"},
	{"{\"autosetups\":[{\"mountPoint\":42}]",
		"Config.autosetups[0].mountPoint: Must be String\n"},
	{"{\"autosetups\":[{\"subscriptions\":42}]",
		"Config.autosetups[0].subscriptions: Must be String or List of strings\n"},
	{"{\"autosetups\":[{\"invalid\":42}]",
		"Config.autosetups[0].invalid: Not expected\n"},
	{"{\"na", "Config: End of input\n"},
};
ARRAY_TEST(load, invalid_config) {
	struct obstack obstack;
	obstack_init(&obstack);
	char *fpath = tmpdir_file("shvcbroker.cpon", _d.cpon);

	char *err;
	size_t errsiz;
	FILE *oldstderr = stderr;
	stderr = open_memstream(&err, &errsiz);
	ck_assert_ptr_nonnull(stderr);

	ck_assert_ptr_null(config_load(fpath, &obstack));

	fclose(stderr);
	stderr = oldstderr;
	ck_assert_str_eq(err, _d.error);
	free(err);

	free(fpath);
	obstack_free(&obstack, NULL);
}

TEST(load, no_such_file) {
	struct obstack obstack;
	obstack_init(&obstack);

	char *err;
	size_t errsiz;
	FILE *oldstderr = stderr;
	stderr = open_memstream(&err, &errsiz);
	ck_assert_ptr_nonnull(stderr);

	ck_assert_ptr_null(config_load("/dev/nonexistent", &obstack));

	fclose(stderr);
	stderr = oldstderr;
	ck_assert_str_eq(err,
		"Unable to open configuration file /dev/nonexistent: No such file or directory\n");
	free(err);

	obstack_free(&obstack, NULL);
}
