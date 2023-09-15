#include "broker.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <check.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "tmpdir.h"

int shvbroker_tcp_port;
char *shvbroker_unix_path;

static char *config_path;
static pid_t pid;


static bool isbound(int port) {
	struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	struct addrinfo *addrs;
	char *p;
	ck_assert_int_ne(asprintf(&p, "%d", port), -1);
	int res = getaddrinfo("localhost", p, &hints, &addrs);
	free(p);
	if (res != 0)
		return false;

	bool success = false;
	struct addrinfo *addr;
	int sock;
	for (addr = addrs; !success && addr != NULL; addr = addr->ai_next) {
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock == -1)
			continue;
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) != -1)
			success = true;
		close(sock);
	}

	freeaddrinfo(addrs);
	return success;
}

static int empty_port(void) {
	while (true) {
		int res = 3600 + (rand() % 1000);
		if (!isbound(res)) {
			return res;
		}
	}
}


void setup_shvbroker(void) {
	setup_tmpdir();
	config_path = tmpdir_path("shvc-brokerconfig.ini");
	FILE *f = fopen(config_path, "w");
	ck_assert_ptr_nonnull(f);
	fputs("[listen]\n", f);
	shvbroker_tcp_port = empty_port();
	fprintf(f, "internet = tcp://localhost:%d\n", shvbroker_tcp_port);
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

	/* Wait for unix socket to be ready */
	struct stat stbuf;
	while (stat(shvbroker_unix_path, &stbuf) == -1 && errno == ENOENT) {}
	/* Wait for the TCP socket to be ready */
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
