#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <alloca.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <shv/rpctransport.h>
#include "shvc_config.h"

#ifndef SHVC_TCP

rpcclient_t rpcclient_tcp_new(
	const char *location, int port, enum rpcstream_proto proto) {
	return NULL;
}

rpcserver_t rpcserver_tcp_new(
	const char *location, int port, enum rpcstream_proto proto) {
	return NULL;
}

#else


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

struct client {
	const char *location;
	int port;
};

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

void tcp_client_disconnect(void *cookie, int fd[2], bool destroy) {
	struct client *c = cookie;
	if (fd[0] != -1)
		close(fd[0]);
	if (destroy)
		free(c);
}

static size_t tcp_client_peername(void *cookie, int fd[2], char *buf, size_t size) {
	struct client *c = cookie;
	return snprintf(buf, size, "tcp:%s:%d", c->location, c->port);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = tcp_client_connect,
	.disconnect = tcp_client_disconnect,
	.flush = tcp_flush,
	.peername = tcp_client_peername,
	.contrack = true,
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

struct server {
	struct rpcserver pub;
	int fd;
	enum rpcstream_proto proto;
};

void tcp_server_disconnect(void *cookie, int fd[2], bool destroy) {
	struct client *c = cookie;
	if (fd[0] != -1)
		close(fd[0]);
	if (destroy)
		free(c);
}

static size_t tcp_server_peername(void *cookie, int fd[2], char *buf, size_t size) {
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(addr);
	if (getpeername(fd[0], (struct sockaddr *)&addr, &addr_len) == -1 ||
		getnameinfo((struct sockaddr *)&addr, addr_len, host, sizeof(host),
			serv, sizeof(serv), NI_NUMERICSERV) == -1)
		return snprintf(buf, size, "tcp:NOT-CONNECTED");
	return snprintf(buf, size, "tcp:%s:%s", host, serv);
}

static const struct rpcclient_stream_funcs sserver = {
	.disconnect = tcp_server_disconnect,
	.flush = tcp_flush,
	.peername = tcp_server_peername,
	.contrack = true,
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

#endif
