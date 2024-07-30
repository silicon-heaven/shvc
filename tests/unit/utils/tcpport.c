#include "tcpport.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <check.h>
#include <netdb.h>
#include "shvc_config.h"


bool tcpport_isbound(int port) {
#ifdef SHVC_TCP
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
#else
	return true;
#endif
}

int tcpport_empty(void) {
	while (true) {
		int res = 3600 + (rand() % 1000);
		if (!tcpport_isbound(res)) {
			return res;
		}
	}
}
