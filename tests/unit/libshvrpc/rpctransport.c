#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <pty.h>
#include <shv/cp_unpack.h>
#include <shv/rpctransport.h>
#include "shvc_config.h"

#define SUITE "rpctransport"
#include <check_suite.h>
#include "tcpport.h"
#include "tmpdir.h"


static void pollfd(int fd) {
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN | POLLHUP,
	};
	ck_assert_int_eq(poll(&pfd, 1, -1), 1);
}

static void test_send(rpcclient_t c, int val) {
	ck_assert(cp_pack_int(rpcclient_pack(c), val));
	ck_assert(rpcclient_sendmsg(c));
}

static void test_receive(rpcclient_t c, int val) {
	pollfd(rpcclient_pollfd(c));
	ck_assert_int_eq(rpcclient_nextmsg(c), RPCC_MESSAGE);
	int rval;
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(c), &item, rval));
	ck_assert_int_eq(rval, val);
	ck_assert(rpcclient_validmsg(c));
}

#ifdef SHVC_TCP
TEST_CASE(tcp) {}

static void *tcp_client(void *arg) {
	int *port = arg;
	rpcclient_t c = rpcclient_tcp_new("localhost", *port, RPCSTREAM_P_BLOCK);
	ck_assert_ptr_nonnull(c);
	ck_assert(rpcclient_reset(c));
	test_send(c, 42);
	test_receive(c, 24);
	rpcclient_destroy(c);
	return NULL;
}
TEST(tcp, tcp_exchange) {
	int port = tcpport_empty();
	rpcserver_t s = rpcserver_tcp_new("localhost", port, RPCSTREAM_P_BLOCK);
	ck_assert_ptr_nonnull(s);

	pthread_t thread;
	pthread_create(&thread, NULL, tcp_client, &port);

	rpcclient_t c = rpcserver_accept(s);
	ck_assert_ptr_nonnull(c);
	test_receive(c, 42);
	test_send(c, 24);

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
}


TEST_CASE(unx, setup_tmpdir, teardown_tmpdir) {}

static void *unix_client(void *arg) {
	char *location = arg;
	rpcclient_t c = rpcclient_unix_new(location, RPCSTREAM_P_SERIAL);
	ck_assert_ptr_nonnull(c);
	ck_assert(rpcclient_reset(c));
	test_send(c, 42);
	test_receive(c, 24);
	rpcclient_destroy(c);
	return NULL;
}
TEST(unx, unix_exchange) {
	char *location = tmpdir_path("socket");
	rpcserver_t s = rpcserver_unix_new(location, RPCSTREAM_P_SERIAL);
	ck_assert_ptr_nonnull(s);

	pthread_t thread;
	pthread_create(&thread, NULL, unix_client, location);

	rpcclient_t c = rpcserver_accept(s);
	ck_assert_ptr_nonnull(c);
	test_receive(c, 42);
	test_send(c, 24);

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
	free(location);
}
#endif


TEST_CASE(tty) {}

struct ttyctx {
	int a, b;
};
static void *tty_ptyex(void *ctx) {
	struct ttyctx *c = ctx;
	struct pollfd pfds[] = {
		{.fd = c->a, .events = POLLIN},
		{.fd = c->b, .events = POLLIN},
	};
	while (pfds[0].events && pfds[1].events) {
		ck_assert_int_gt(poll(pfds, 2, -1), 0);
		for (int i = 0; i < 2; i++) {
			if (pfds[i].revents & POLLIN) {
				uint8_t buf[BUFSIZ];
				int ret = read(pfds[i].fd, buf, BUFSIZ);
				ck_assert_int_ge(ret, 0);
				ck_assert_int_eq(write(pfds[i ? 0 : 1].fd, buf, ret), ret);
			}
			if (pfds[i].revents & (POLLHUP | POLLERR)) {
				pfds[i].events = 0;
			}
		}
	}
	close(c->a);
	close(c->b);
	return NULL;
}
static void pollreset(rpcclient_t c) {
	enum rpcclient_msg_type mt = RPCC_NOTHING;
	while (mt == RPCC_NOTHING) {
		pollfd(rpcclient_pollfd(c));
		mt = rpcclient_nextmsg(c);
	}
	ck_assert_int_eq(mt, RPCC_RESET);
}
static void *tty_client(void *arg) {
	rpcclient_t c = arg;
	pollreset(c);
	pollreset(c);
	test_send(c, 42);
	test_receive(c, 24);
	rpcclient_destroy(c);
	return NULL;
}
TEST(tty, tty_exchange) {
	int am, as, bm, bs;
	char apty[PATH_MAX], bpty[PATH_MAX];
	ck_assert_int_eq(openpty(&am, &as, apty, NULL, NULL), 0);
	ck_assert_int_eq(openpty(&bm, &bs, bpty, NULL, NULL), 0);

	rpcserver_t s = rpcserver_tty_new(apty, 115200, RPCSTREAM_P_SERIAL_CRC);
	ck_assert_ptr_nonnull(s);
	/* TTY doesn't have connection tracking and thus we do not need client to
	 * run to accept connection.
	 */
	pollfd(rpcserver_pollfd(s));
	rpcclient_t c = rpcserver_accept(s);
	ck_assert_ptr_nonnull(c);
	/* We destroy this client to test that we can accept a new one. */
	rpcclient_destroy(c);
	c = rpcserver_accept(s);
	ck_assert_ptr_nonnull(c);

	rpcclient_t cc = rpcclient_tty_new(bpty, 115200, RPCSTREAM_P_SERIAL_CRC);
	ck_assert_ptr_nonnull(cc);
	ck_assert(rpcclient_reset(cc));
	pthread_t thread;
	ck_assert_int_eq(pthread_create(&thread, NULL, tty_client, cc), 0);
	sched_yield();

	close(as);
	close(bs);
	pthread_t ptythread;
	struct ttyctx ttyctx = {.a = am, .b = bm};
	ck_assert_int_eq(pthread_create(&ptythread, NULL, tty_ptyex, &ttyctx), 0);

	pollreset(c);
	test_receive(c, 42);
	test_send(c, 24);

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
	pthread_join(ptythread, NULL);
}
