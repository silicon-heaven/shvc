#include <shv/rpcclient.h>
#include <shv/chainpack.h>
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
#include <netdb.h>


struct rpcclient_stream {
	struct rpcclient c;
	/* Read */
	FILE *rf;
	size_t rmsgoff;
	/* Write */
	int wfd;
	FILE *fbuf;
	char *buf;
	size_t bufsiz;
};


static ssize_t xread(int fd, char *buf, size_t siz) {
	ssize_t i;
	do
		i = read(fd, buf, siz);
	while (i == -1 && errno == EINTR);
	return i;
}

static int readc(int fd) {
	uint8_t v;
	if (xread(fd, (void *)&v, 1) < 0)
		return -1;
	return v;
}

static ssize_t cookie_read(void *cookie, char *buf, size_t size) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)cookie;
	if (c->rmsgoff < size)
		/* Read up to the end of the message and then signal EOF by reading 0. */
		size = c->rmsgoff;
	ssize_t i = xread(c->c.rfd, buf, size);
	if (i > 0)
		c->rmsgoff -= i;
	return i;
}

static size_t cp_unpack_stream(void *ptr, struct cpitem *item) {
	struct rpcclient_stream *c = ptr - offsetof(struct rpcclient_stream, c.unpack);
	size_t res = chainpack_unpack(c->rf, item);
	rpcclient_log_item(c->c.logger, item);
	return res;
}

static ssize_t read_size(int fd) {
	int c;
	if ((c = readc(fd)) == -1)
		return -1;
	unsigned bytes = chainpack_int_bytes(c);
	size_t res = chainpack_uint_value1(c, bytes);
	for (unsigned i = 1; i < bytes; i++) {
		if ((c = readc(fd)) == -1)
			return -1;
		res = (res << 8) | c;
	}
	return res;
}

static bool msgfetch_stream(struct rpcclient *client, bool newmsg) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	bool ok = true;
	if (newmsg) {
		ssize_t msgsiz = read_size(c->c.rfd);
		if (msgsiz == -1)
			return false;
		c->rmsgoff = msgsiz;
		clearerr(c->rf);
		if (ok) {
			if (fgetc(c->rf) != CP_ChainPack)
				ok = false;
			else
				rpcclient_log_lock(c->c.logger, true);
		}
		rpcclient_last_receive_update(client);
	} else {
		rpcclient_last_receive_update(client);
		rpcclient_log_unlock(c->c.logger);
		/* Flush the rest of the message */
		char buf[BUFSIZ];
		while (fread(buf, 1, BUFSIZ, c->rf) > 0)
			;
	}
	return ok;
}

static size_t cp_pack_stream(void *ptr, const struct cpitem *item) {
	struct rpcclient_stream *c = ptr - offsetof(struct rpcclient_stream, c.pack);
	if (ftell(c->fbuf) == 0)
		rpcclient_log_lock(c->c.logger, false);
	rpcclient_log_item(c->c.logger, item);
	return chainpack_pack(c->fbuf, item);
}

static bool write_size(int fd, size_t len) {
	unsigned bytes = chainpack_w_uint_bytes(len);
	for (unsigned i = 0; i < bytes; i++) {
		uint8_t c = i == 0 ? chainpack_w_uint_value1(len, bytes)
						   : 0xff & (len >> (8 * (bytes - i - 1)));
		int res;
		do
			res = write(fd, &c, 1);
		while (res == -1 && errno == EINTR);
		if (res == -1)
			return false;
	}
	return true;
}

static bool msgflush_stream(struct rpcclient *client, bool send) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	rpcclient_log_unlock(c->c.logger);
	fflush(c->fbuf);
	if (send) {
		size_t len = ftell(c->fbuf);
		if (!write_size(c->wfd, len + 1))
			return false;
		int res;
		uint8_t cpf = CP_ChainPack;
		do
			res = write(c->wfd, &cpf, 1);
		while (res == -1 && errno == EINTR);
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
		rpcclient_last_send_update(client);
	}
	fseek(c->fbuf, 0, SEEK_SET);
	return true;
}

static void disconnect(rpcclient_t client) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	fclose(c->rf);
	fclose(c->fbuf);
	free(c->buf);
	close(c->c.rfd);
	if (c->c.rfd != c->wfd)
		close(c->wfd);
	free(c);
}

rpcclient_t rpcclient_stream_new(int readfd, int writefd) {
	struct rpcclient_stream *res = malloc(sizeof *res);
	assert(res);
	FILE *f = fopencookie(res, "r", (cookie_io_functions_t){.read = cookie_read});
	assert(f);
	*res = (struct rpcclient_stream){
		.c =
			(struct rpcclient){
				.rfd = readfd,
				.unpack = cp_unpack_stream,
				.msgfetch = msgfetch_stream,
				.pack = cp_pack_stream,
				.msgflush = msgflush_stream,
				.disconnect = disconnect,
				.logger = NULL,
			},
		.rf = f,
		.rmsgoff = 0,
		.wfd = writefd,
		.bufsiz = 0,
	};
	res->fbuf = open_memstream(&res->buf, &res->bufsiz);
	assert(res->fbuf);
	return &res->c;
}

rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port) {
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

	struct addrinfo *addr;
	int sock;
	for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock == -1)
			continue;
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) != -1)
			break;
		close(sock);
	}

	freeaddrinfo(addrs);
	return addr ? rpcclient_stream_new(sock, sock) : NULL;
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
