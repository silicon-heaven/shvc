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
	int errnum;
	/* Read */
	int rfd;
	FILE *rf;
	size_t rmsgoff;
	/* Write */
	int wfd;
	FILE *fbuf;
	char *buf;
	size_t bufsiz;
};


static ssize_t xread(struct rpcclient_stream *c, char *buf, size_t siz) {
	ssize_t i;
	do
		i = read(c->rfd, buf, siz);
	while (i == -1 && errno == EINTR);
	if (i == -1)
		c->errnum = errno;
	return i;
}

static int readc(struct rpcclient_stream *c) {
	uint8_t v;
	if (xread(c, (void *)&v, 1) != 1) {
		c->errnum = ENODATA;
		return -1;
	}
	return v;
}

static bool xwrite(struct rpcclient_stream *c, const void *buf, size_t siz) {
	uint8_t *data = (uint8_t *)buf;
	while (siz > 0) {
		ssize_t i = write(c->wfd, data, siz);
		if (i == -1) {
			if (errno != EINTR) {
				c->errnum = errno;
				return false;
			}
		} else {
			data += i;
			siz -= i;
		}
	}
	return true;
}

static ssize_t cookie_read(void *cookie, char *buf, size_t size) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)cookie;
	if (c->rmsgoff < size)
		/* Read up to the end of the message and then signal EOF by reading 0. */
		size = c->rmsgoff;
	ssize_t i = xread(c, buf, size);
	if (i > 0)
		c->rmsgoff -= i;
	return i;
}

static void cp_unpack_stream(void *ptr, struct cpitem *item) {
	struct rpcclient_stream *c = ptr - offsetof(struct rpcclient_stream, c.unpack);
	chainpack_unpack(c->rf, item);
	rpclogger_log_item(c->c.logger, item);
}

static ssize_t read_size(struct rpcclient_stream *c) {
	int v;
	if ((v = readc(c)) == -1)
		return -1;
	unsigned bytes = chainpack_int_bytes(v);
	size_t res = chainpack_uint_value1(v, bytes);
	for (unsigned i = 1; i < bytes; i++) {
		if ((v = readc(c)) == -1)
			return -1;
		res = (res << 8) | v;
	}
	return res;
}

static void flushmsg(struct rpcclient_stream *c) {
	/* Flush the rest of the message. Read up to the EOF. */
	char buf[BUFSIZ];
	while (fread(buf, 1, BUFSIZ, c->rf) > 0) {}
}

static bool nextmsg_stream(struct rpcclient_stream *c) {
	bool ok = true;
	ssize_t msgsiz = read_size(c);
	if (msgsiz == -1)
		return false;
	c->rmsgoff = msgsiz;
	clearerr(c->rf);
	if (fgetc(c->rf) != CP_ChainPack) {
		flushmsg(c);
		rpcclient_last_receive_update(&c->c); /* Still valid receive */
		return false;
	}
	rpclogger_log_lock(c->c.logger, true);
	rpcclient_last_receive_update(&c->c);
	return ok;
}

static bool validmsg_stream(struct rpcclient_stream *c) {
	rpcclient_last_receive_update(&c->c);
	rpclogger_log_unlock(c->c.logger);
	flushmsg(c);
	rpcclient_last_receive_update(&c->c);
	return true; /* Always valid for stream as we relly on lower layer */
}

static bool cp_pack_stream(void *ptr, const struct cpitem *item) {
	struct rpcclient_stream *c = ptr - offsetof(struct rpcclient_stream, c.pack);
	if (ftell(c->fbuf) == 0)
		rpclogger_log_lock(c->c.logger, false);
	rpclogger_log_item(c->c.logger, item);
	return chainpack_pack(c->fbuf, item) > 0;
}

static bool write_size(struct rpcclient_stream *c, size_t len) {
	unsigned bytes = chainpack_w_uint_bytes(len);
	for (unsigned i = 0; i < bytes; i++) {
		uint8_t v = i == 0 ? chainpack_w_uint_value1(len, bytes)
						   : 0xff & (len >> (8 * (bytes - i - 1)));
		if (!xwrite(c, &v, 1))
			return false;
	}
	return true;
}

static bool msgflush_stream(struct rpcclient_stream *c, bool send) {
	rpclogger_log_unlock(c->c.logger);
	fflush(c->fbuf);
	if (send) {
		size_t len = ftell(c->fbuf);
		if (!write_size(c, len + 1))
			return false;
		uint8_t cpf = CP_ChainPack;
		if (!xwrite(c, &cpf, 1) || !xwrite(c, c->buf, len))
			return false;
		rpcclient_last_send_update(&c->c);
	}
	fseek(c->fbuf, 0, SEEK_SET);
	return true;
}

static void destroy(rpcclient_t client) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	fclose(c->rf);
	fclose(c->fbuf);
	free(c->buf);
	close(c->rfd);
	if (c->rfd != c->wfd)
		close(c->wfd);
	free(c);
}

static int ctl_stream(struct rpcclient *client, enum rpcclient_ctlop op) {
	struct rpcclient_stream *c = (struct rpcclient_stream *)client;
	switch (op) {
		case RPCC_CTRLOP_DESTROY:
			destroy(client);
			return true;
		case RPCC_CTRLOP_ERRNO:
			return c->errnum;
		case RPCC_CTRLOP_NEXTMSG:
			return nextmsg_stream(c);
		case RPCC_CTRLOP_VALIDMSG:
			return validmsg_stream(c);
		case RPCC_CTRLOP_SENDMSG:
			return msgflush_stream(c, true);
		case RPCC_CTRLOP_DROPMSG:
			return msgflush_stream(c, false);
		case RPCC_CTRLOP_POLLFD:
			return c->rfd;
	}
	abort(); /* This should not happen -> implementation error */
}


rpcclient_t rpcclient_stream_new(int readfd, int writefd) {
	struct rpcclient_stream *res = malloc(sizeof *res);
	assert(res);
	FILE *f = fopencookie(res, "r", (cookie_io_functions_t){.read = cookie_read});
	assert(f);
	*res = (struct rpcclient_stream){
		.c =
			(struct rpcclient){
				.ctl = ctl_stream,
				.unpack = cp_unpack_stream,
				.pack = cp_pack_stream,
				.logger = NULL,
			},
		.rfd = readfd,
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
