#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <shv/rpctransport.h>

struct server {
	struct rpcserver pub;
	int fd;
	enum rpcstream_proto proto;
};

static size_t unix_peername(char *buf, size_t size, const char *location) {
	const char *prefix = "unix:";
	char *b = stpncpy(buf, prefix, size);
	strncpy(b, location, size - (b - buf));
	return strlen(prefix) + strlen(location);
}

/* Client *********************************************************************/

static bool unix_client_connect(void *cookie, int fd[2]) {
	char *location = cookie;
	int nfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (nfd == -1)
		return false;
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, location, sizeof(addr.sun_path) - 1);

	if (connect(nfd, (const struct sockaddr *)&addr, sizeof(addr)) != -1) {
		fd[1] = fd[0] = nfd;
	} else
		close(nfd);
	return false;
}

static void unix_client_disconnect(void *cookie, int fd[2], bool destroy) {
	char *location = cookie;
	close(fd[0]);
	if (destroy)
		free(location);
}

static size_t unix_client_peername(void *cookie, int fd[2], char *buf, size_t size) {
	char *location = cookie;
	return unix_peername(buf, size, location);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = unix_client_connect,
	.disconnect = unix_client_disconnect,
	.peername = unix_client_peername,
};

rpcclient_t rpcclient_unix_new(const char *location, enum rpcstream_proto proto) {
	char *l = strdup(location);
	return rpcclient_stream_new(&sclient, l, proto, -1, -1);
}

/* Server *********************************************************************/

static size_t unix_server_peername(void *cookie, int fd[2], char *buf, size_t size) {
	struct sockaddr_un addr;
	socklen_t addr_len = sizeof(addr);
	if (getsockname(fd[0], (struct sockaddr *)&addr, &addr_len) == -1)
		strcpy(addr.sun_path, "NOT-CONNECTED");
	return snprintf(buf, size, "unix:%s", addr.sun_path);
}

static const struct rpcclient_stream_funcs sserver = {
	.peername = unix_server_peername,
};

static int unix_server_ctrl(struct rpcserver *server, enum rpcserver_ctrlop op) {
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

static rpcclient_t unix_server_accept(struct rpcserver *server) {
	struct server *s = (struct server *)server;
	int fd = accept(s->fd, NULL, NULL);
	if (fd == -1)
		return NULL;
	return rpcclient_stream_new(&sserver, NULL, s->proto, fd, fd);
}

rpcserver_t rpcserver_unix_new(const char *location, enum rpcstream_proto proto) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return NULL;
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, location, sizeof(addr.sun_path) - 1);

	if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1 ||
		listen(fd, 8) == -1) {
		close(fd);
		return NULL;
	}

	struct server *res = malloc(sizeof *res);
	*res = (struct server){
		.pub = {.ctrl = unix_server_ctrl, .accept = unix_server_accept},
		.fd = fd,
		.proto = proto,
	};
	return &res->pub;
}
