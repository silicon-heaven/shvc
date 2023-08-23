#include "shv/cp.h"
#include <shv/rpcclient.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


struct rpcclient_stream {
	struct rpcclient c;
	/* Read */
	int rfd;
	FILE *rf;
	ssize_t rmsgoff;
	/* Write */
	int wfd;
	FILE *fbuf;
	char *buf;
	size_t bufsiz;
};


ssize_t cookie_read(void *cookie, char *buf, size_t size) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)cookie;
	if (c->rmsgoff < 0)
		/* Read only byte by byte when receiving message length to not feed too
		 * much bytes to the FILE.
		 */
		size = 1;
	else if (c->rmsgoff < size)
		/* Read up to the end of the message and then signal EOF by reading 0. */
		size = c->rmsgoff;
	ssize_t i;
	do
		i = read(c->rfd, buf, size);
	while (i == -1 && errno == EINTR);
	c->rmsgoff -= i;
	return i;
}

static size_t cp_unpack_stream(void *ptr, struct cpitem *item) {
	struct rpcclient_stream *c = ptr - offsetof(struct rpcclient, unpack);
	// TODO log
	return chainpack_unpack(c->rf, item);
}

static bool nextmsg_stream(struct rpcclient *client) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	c->rmsgoff = -1;
	unsigned long long rmsgoff;
	bool ok;
	_chainpack_unpack_uint(c->rf, &rmsgoff, &ok);
	/* Note: we use stack allocated variable here because cookie_read might be
	 * called multiple times and we always want to have -1 in rmsgoff;
	 */
	c->rmsgoff = rmsgoff;
	if (ok && fgetc(c->rf) != CP_ChainPack)
		ok = false;
	// TODO log lock
	return ok;
}

static ssize_t cp_pack_stream(void *ptr, const struct cpitem *item) {
	struct rpcclient_stream *c = ptr - offsetof(struct rpcclient, unpack);
	// TODO log
	return chainpack_pack(c->fbuf, item);
}

static bool sendmsg_stream(struct rpcclient *client) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	fflush(c->fbuf);
	size_t len = ftell(c->fbuf);
	_chainpack_pack_uint(c->rf, len);
	ssize_t i = 0;
	while (i < len) {
		ssize_t r = write(c->wfd, c->buf, len);
		if (r == -1) {
			if (errno == EINTR)
				continue;
			return false;
		}
		i += r;
	}
	fseek(c->fbuf, 0, SEEK_SET);
	return true;
}

static void disconnect(rpcclient_t client) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	fclose(c->rf);
	fclose(c->fbuf);
	close(c->rfd);
	if (c->rfd != c->wfd)
		close(c->wfd);
	free(c);
}

rpcclient_t rpcclient_stream_new(int readfd, int writefd) {
	// TODO check for allocation and open failures
	struct rpcclient_stream *res = malloc(sizeof *res);
	FILE *f = fopencookie(res, "r", (cookie_io_functions_t){.read = cookie_read});
	*res = (struct rpcclient_stream){
		.c =
			(struct rpcclient){
				.unpack = cp_unpack_stream,
				.nextmsg = nextmsg_stream,
				.pack = cp_pack_stream,
				.sendmsg = sendmsg_stream,
				.disconnect = disconnect,
				.logger = NULL,
			},
		.rf = f,
		.rfd = readfd,
		.rmsgoff = 0,
		.wfd = writefd,
		.bufsiz = 0,
	};
	res->fbuf = open_memstream(&res->buf, &res->bufsiz);
	return &res->c;
}

rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port) {
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	// TODO connect
	return rpcclient_stream_new(fd, fd);
}

rpcclient_t rpcclient_stream_unix_connect(const char *location) {
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	const size_t addrlen =
		sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;
	strncpy(addr.sun_path, location, addrlen);
	addr.sun_path[addrlen] = '\0';
	if (connect(sock, &addr, sizeof addr) == -1) {
		close(sock);
		return NULL;
	}

	return rpcclient_stream_new(sock, sock);
}
