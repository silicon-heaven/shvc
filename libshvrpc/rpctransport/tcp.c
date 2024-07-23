#include <shv/rpcclient_stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

struct ctx {
	const char *location;
	int port;
};


static void tcp_connect(void *cookie, int fd[2]) {
	struct ctx *c = cookie;
	struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo *addrs;
	char *p;
	assert(asprintf(&p, "%d", c->port) != -1);
	int res = getaddrinfo(c->location, p, &hints, &addrs);
	free(p);
	if (res != 0)
		return;
	for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
		fd[0] = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (fd[0] == -1)
			continue;
		if (connect(fd[0], addr->ai_addr, addr->ai_addrlen) != -1)
			break;
		close(fd[0]);
		fd[0] = -1;
	}
	freeaddrinfo(addrs);
	if (fd[0] == -1)
		return;
	fd[1] = fd[0];
}

void tcp_flush(void *cookie, int fd) {
	int flag = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	flag = 0;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
}

void tcp_free(void *cookie) {
	struct ctx *c = cookie;
	free(c);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = tcp_connect,
	.flush = tcp_flush,
	.free = tcp_free,
};

rpcclient_t rpcclient_tcp_new(
	const char *location, int port, enum rpcstream_proto proto) {
	struct ctx *res = malloc(sizeof *res);
	*res = (struct ctx){
		.location = location,
		.port = port,
	};
	return rpcclient_stream_new(&sclient, res, proto, -1, -1);
}
