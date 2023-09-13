#include "broker.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <check.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include "tmpdir.h"

int shvbroker_tcp_port;
char *shvbroker_unix_path;

static char *config_path;
static pid_t pid;


static bool isbound(int port) {
	bool res = false;
	char *expect;
	ck_assert_int_ne(
		asprintf(&expect,
			"00000000000000000000000000000000:%04X 00000000000000000000000000000000:0000",
			shvbroker_tcp_port),
		-1);
	char *l = NULL;
	size_t n;
	FILE *f = fopen("/proc/net/tcp6", "r");
	while (getline(&l, &n, f) != -1) {
		if (strstr(l, expect) != NULL) {
			res = true;
			break;
		}
	}
	fclose(f);
	free(l);
	free(expect);
	return res;
}

static int empty_port(void) {
	while (true) {
		int res = 3600 + (rand() % 1000);
		if (!isbound(res))
			return res;
	}
}


void setup_shvbroker(void) {
	setup_tmpdir();
	config_path = tmpdir_path("shvc-brokerconfig.ini");
	FILE *f = fopen(config_path, "w");
	ck_assert_ptr_nonnull(f);
	fputs("[listen]\n", f);
	shvbroker_tcp_port = empty_port();
	fprintf(f, "internet = tcp://[::]:%d\n", shvbroker_tcp_port);
	shvbroker_unix_path = tmpdir_path("shvbroker.sock");
	fprintf(f, "unix = localsocket:%s\n", shvbroker_unix_path);
	fputs("[users.admin]\n", f);
	fputs("password = admin!123\n", f);
	fputs("roles = admin\n", f);
	fputs("[users.shaadmin]\n", f);
	fputs("sha1pass = 57a261a7bcb9e6cf1db80df501cdd89cee82957e\n", f);
	fputs("roles = admin\n", f);
	fputs("[roles.admin]\n", f);
	fputs("rules = admin\n", f);
	fputs("access = dev\n", f);
	fputs("[rules.admin]\n", f);
	fclose(f);

	char *args[] = {PYSHVBROKER, "-vvc", config_path, NULL};

	ck_assert_int_eq(posix_spawn(&pid, PYSHVBROKER, NULL, NULL, args, NULL), 0);

	while (!isbound(shvbroker_tcp_port)) {}
}

void teardown_shvbroker(void) {
	unlink(config_path);
	free(config_path);
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
	unlink(shvbroker_unix_path);
	free(shvbroker_unix_path);
}
