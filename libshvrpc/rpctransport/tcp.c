#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <shv/rpctransport.h>

struct client {
	const char *location;
	int port;
};

struct server {
	struct rpcserver pub;
	int fd;
	enum rpcstream_proto proto;
};


static struct addrinfo *tcp_addrinfo(const char *location, int port) {
	struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo *addrs;
	char *p;
	assert(asprintf(&p, "%d", port) != -1);
	int res = getaddrinfo(location, p, &hints, &addrs);
	free(p);
	if (res != 0)
		return NULL;
	return addrs;
}

void tcp_flush(void *cookie, int fd) {
	int flag = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	flag = 0;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
}

/* Client *********************************************************************/

static bool tcp_client_connect(void *cookie, int fd[2]) {
	struct client *c = cookie;
	struct addrinfo *addrs = tcp_addrinfo(c->location, c->port);
	if (addrs == NULL)
		return false;
	int nfd = -1;
	for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
		nfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (nfd != -1) {
			if (connect(nfd, addr->ai_addr, addr->ai_addrlen) != -1)
				break;
			close(nfd);
			nfd = -1;
		}
	}
	freeaddrinfo(addrs);
	if (nfd != -1)
		fd[0] = fd[1] = nfd;
	return false;
}

void tcp_client_free(void *cookie) {
	struct client *c = cookie;
	free(c);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = tcp_client_connect,
	.flush = tcp_flush,
	.free = tcp_client_free,
};

rpcclient_t rpcclient_tcp_new(
	const char *location, int port, enum rpcstream_proto proto) {
	struct client *res = malloc(sizeof *res);
	*res = (struct client){
		.location = location,
		.port = port,
	};
	return rpcclient_stream_new(&sclient, res, proto, -1, -1);
}

/* Server *********************************************************************/

static bool tcp_server_connect(void *cookie, int fd[2]) {
	/* Dummy connect just to transfer socket ownership on the stream client */
	return false;
}

static const struct rpcclient_stream_funcs sserver = {
	.connect = tcp_server_connect,
	.flush = tcp_flush,
};

static int tcp_server_ctrl(struct rpcserver *server, enum rpcserver_ctrlop op) {
	struct server *s = (struct server *)server;
	switch (op) {
		case RPCS_CTRLOP_POLLFD:
			return s->fd;
		case RPCS_CTRLOP_DESTROY:
			close(s->fd);
			free(s);
			return 0;
	}
	abort();
}

static rpcclient_t tcp_server_accept(struct rpcserver *server) {
	struct server *s = (struct server *)server;
	int fd = accept(s->fd, NULL, NULL);
	if (fd == -1)
		return NULL;
	return rpcclient_stream_new(&sserver, NULL, s->proto, fd, fd);
}

rpcserver_t rpcserver_tcp_new(
	const char *location, int port, enum rpcstream_proto proto) {
	int fd = -1;
	struct addrinfo *addrs = tcp_addrinfo(location, port);
	if (addrs == NULL)
		return NULL;
	for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
		fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (fd == -1)
			continue;
		int yes = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ||
			bind(fd, addr->ai_addr, addr->ai_addrlen) == -1 ||
			listen(fd, 8) == -1) {
			close(fd);
			fd = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(addrs);
	if (fd == -1)
		return NULL;

	struct server *res = malloc(sizeof *res);
	*res = (struct server){
		.pub = {.ctrl = tcp_server_ctrl, .accept = tcp_server_accept},
		.fd = fd,
		.proto = proto,
	};
	return &res->pub;
}
