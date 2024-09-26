#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <syslog.h>
#include <termios.h>
#include <shv/rpctransport.h>

struct client {
	const char *location;
	unsigned baudrate;
};

struct server {
	struct rpcserver pub;
	const char *location;
	unsigned baudrate;
	enum rpcstream_proto proto;
	int evfd;
	pthread_t thread;
};

static int tty_connect(const char *location, unsigned baudrate) {
	int fd = open(location, O_RDWR);
	if (fd == -1)
		return -1;
	if (isatty(fd)) {
		struct termios t;
		tcgetattr(fd, &t);
		cfmakeraw(&t);
		cfsetispeed(&t, baudrate);
		cfsetospeed(&t, baudrate);
		t.c_cflag |= CRTSCTS;
		if (tcsetattr(fd, TCSANOW, &t) == -1) // GCOVR_EXCL_BR_LINE
			syslog(							  // GCOVR_EXCL_LINE
				LOG_WARNING, "%s: Failed to set TTY mode: %m", location);
	} else {
		syslog(LOG_WARNING, "%s: Not a TTY", location); // GCOVR_EXCL_LINE
		close(fd);										// GCOVR_EXCL_LINE
		return -1;										// GCOVR_EXCL_LINE
	}
	/* Flush everything that was previously buffered to start new. */
	struct pollfd pfd = {.fd = fd, .events = POLLIN};
	while (poll(&pfd, 1, 0) == 1) {
		uint8_t buf[BUFSIZ];
		if (read(fd, buf, BUFSIZ) <= 0)
			return -1;
	}
	return fd;
}

/* Client *********************************************************************/

static bool tty_client_connect(void *cookie, int fd[2]) {
	struct client *c = (struct client *)cookie;
	fd[0] = fd[1] = tty_connect(c->location, c->baudrate);
	return true;
}

static void tty_client_disconnect(void *cookie, int fd[2], bool destroy) {
	struct client *c = (struct client *)cookie;
	if (fd[0] != -1)
		close(fd[0]);
	if (destroy)
		free(c);
}

static size_t tty_client_peername(void *cookie, int fd[2], char *buf, size_t size) {
	struct client *c = (struct client *)cookie;
	return snprintf(buf, size, "tty:%s", c->location);
}

void tty_client_free(void *cookie) {
	struct ctx *c = (struct ctx *)cookie;
	free(c);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = tty_client_connect,
	.disconnect = tty_client_disconnect,
	.peername = tty_client_peername,
	.contrack = false,
};

rpcclient_t rpcclient_tty_new(
	const char *location, unsigned baudrate, enum rpcstream_proto proto) {
	struct client *res = malloc(sizeof *res);
	*res = (struct client){
		.location = location,
		.baudrate = baudrate,
	};
	return rpcclient_stream_new(&sclient, res, proto, -1, -1);
}

/* Server *********************************************************************/

// TODO on platforms that support inotify we can use that to wait for TTY.

static const struct rpcclient_stream_funcs sserver;

static void *tty_server_loop(void *ctx) {
	struct server *s = ctx;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	int fd;
	unsigned cnt = 0;
	while ((fd = tty_connect(s->location, s->baudrate)) == -1) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		sleep(MIN((cnt++ >> 4) + 1, 15));
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	}

	rpcclient_t c = rpcclient_stream_new(&sserver, s, s->proto, fd, fd);
	/* We always send reset to ensure that even if we destroyed our client
	 * the reset is propagated.
	 * Note that sending reset can fail if buffer is full; thus we ignore reset
	 * failure because otherwise we would be caught in the infinite loop here of
	 * trying to send reset again and again.
	 */
	__attribute__((unused)) bool _ = rpcclient_reset(c);
	eventfd_write(s->evfd, 42);
	return c;
}

static void tty_server_disconnect(void *cookie, int fd[2], bool destroy) {
	struct server *s = cookie;
	if (fd[0] < 0) /* destroy after explicit disconnect */
		return;
	/* Set to non-block before close to ensure that flush of data won't block. */
	fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK);
	close(fd[0]);
	if (s->evfd != -1)
		pthread_create(&s->thread, NULL, tty_server_loop, s);
}

static size_t tty_server_peername(void *cookie, int fd[2], char *buf, size_t size) {
	struct server *s = (struct server *)cookie;
	return snprintf(buf, size, "tty:%s", s->location);
}

static const struct rpcclient_stream_funcs sserver = {
	.disconnect = tty_server_disconnect,
	.peername = tty_server_peername,
	.contrack = false,
};

static int tty_server_ctrl(struct rpcserver *server, enum rpcserver_ctrlop op) {
	struct server *s = (struct server *)server;
	switch (op) {
		case RPCS_CTRLOP_POLLFD:
			return s->evfd;
		case RPCS_CTRLOP_DESTROY:
			pthread_cancel(s->thread);
			rpcclient_t client = NULL;
			pthread_join(s->thread, (void **)&client);
			close(s->evfd);
			s->evfd = -1; /* This is used to identify server exit */
			if (client != NULL && client != PTHREAD_CANCELED)
				rpcclient_destroy(client);
			free(s);
			return 0;
	}
	abort(); // GCOVR_EXCL_LINE
}

static rpcclient_t tty_server_accept(struct rpcserver *server) {
	struct server *s = (struct server *)server;
	eventfd_t val;
	// TODO do we need to cover for EINTR here?
	eventfd_read(s->evfd, &val);
	eventfd_write(s->evfd, 0);
	rpcclient_t res;
	pthread_join(s->thread, (void **)&res);
	return res;
}

rpcserver_t rpcserver_tty_new(
	const char *location, unsigned baudrate, enum rpcstream_proto proto) {
	struct server *res = malloc(sizeof *res);
	*res = (struct server){
		.pub = {.ctrl = tty_server_ctrl, .accept = tty_server_accept},
		.location = location,
		.baudrate = baudrate,
		.proto = proto,
		.evfd = eventfd(0, 0),
	};
	pthread_create(&res->thread, NULL, tty_server_loop, res);
	return &res->pub;
}
