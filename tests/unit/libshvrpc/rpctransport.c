#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <linux/can/raw.h>
#include <linux/if.h>
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

static void test_receive_int(rpcclient_t c, int val) {
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
	test_receive_int(c, 24);
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
	test_receive_int(c, 42);
	test_send(c, 24);

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
}
#endif


TEST_CASE(unx, setup_tmpdir, teardown_tmpdir) {}

static void *unix_client(void *arg) {
	char *location = arg;
	rpcclient_t c = rpcclient_unix_new(location, RPCSTREAM_P_SERIAL);
	ck_assert_ptr_nonnull(c);
	ck_assert(rpcclient_reset(c));
	test_send(c, 42);
	test_receive_int(c, 24);
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
	test_receive_int(c, 42);
	test_send(c, 24);

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
	free(location);
}


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
	test_receive_int(c, 24);
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
	test_receive_int(c, 42);
	test_send(c, 24);

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
	pthread_join(ptythread, NULL);
}


TEST_CASE(can) {}

const char *longmsg =
	"This is just a long message we will check if we are able to send. We do "
	"this for CAN where multiple messages have to be split to different frames.";
static void *can_client(void *arg) {
	char *location = arg;
	rpcclient_t c = rpcclient_can_new(location, 40, 80);
	ck_assert_ptr_nonnull(c);
	ck_assert(rpcclient_reset(c));
	test_send(c, 42);
	test_receive_int(c, 42);
	test_receive_int(c, 42);

	pollfd(rpcclient_pollfd(c));
	ck_assert_int_eq(rpcclient_nextmsg(c), RPCC_MESSAGE);
	struct cpitem item;
	cpitem_unpack_init(&item);
	char *str = cp_unpack_strdup(rpcclient_unpack(c), &item);
	ck_assert(rpcclient_validmsg(c));
	ck_assert_pstr_eq(str, longmsg);

	rpcclient_destroy(c);
	return NULL;
}
TEST(can, can_exchange) {
	const char *location = getenv("SHVC_TEST_CANIF");
	if (!location)
		return; /* No test interface so we can't perform any tests */
	rpcserver_t s = rpcserver_can_new(location, 40);
	ck_assert_ptr_nonnull(s);

	pthread_t thread;
	pthread_create(&thread, NULL, can_client, (void *)location);

	rpcclient_t c = rpcserver_accept(s);
	ck_assert_ptr_nonnull(c);
	test_receive_int(c, 42);
	test_send(c, 42);
	test_send(c, 42);
	ck_assert(cp_pack_str(rpcclient_pack(c), longmsg));
	ck_assert(rpcclient_sendmsg(c));

	pthread_join(thread, NULL);
	rpcclient_destroy(c);
	rpcserver_destroy(s);
}

static rpcclient_t canraw_c = NULL;
static int canraw_fd = -1;
static void canraw_setup(void) {
	const char *canif = getenv("SHVC_TEST_CANIF");
	if (!canif)
		return; /* No test interface so we can't perform any tests */

	canraw_c = rpcclient_can_new(canif, 4, 9);
	ck_assert_ptr_nonnull(canraw_c);
	ck_assert(rpcclient_reset(canraw_c));

	canraw_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	ck_assert_int_ge(canraw_fd, 0);
	int val = 1;
	ck_assert_int_eq(
		setsockopt(canraw_fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &val, sizeof val), 0);
	struct ifreq ifr;
	strncpy(ifr.ifr_name, canif, sizeof ifr.ifr_name);
	ck_assert_int_eq(ioctl(canraw_fd, SIOCGIFINDEX, &ifr), 0);
	struct sockaddr_can addr = {
		.can_family = AF_CAN, .can_ifindex = ifr.ifr_ifindex};
	ck_assert_int_eq(bind(canraw_fd, (struct sockaddr *)&addr, sizeof addr), 0);
}
static void canraw_teardown(void) {
	if (canraw_fd == -1)
		return;
	close(canraw_fd);
	rpcclient_destroy(canraw_c);
}
TEST_CASE(canraw, canraw_setup, canraw_teardown) {}

#define ckread(PRIO, ...) \
	do { \
		const uint8_t data[] = {4, __VA_ARGS__}; \
		struct canfd_frame frame; \
		ck_assert_int_eq(read(canraw_fd, &frame, CANFD_MTU), CANFD_MTU); \
		ck_assert_int_eq(frame.can_id, 0x609 | (PRIO ? 0 : 0x100)); \
		ck_assert_int_eq(frame.flags, CANFD_FDF | CANFD_BRS); \
		ck_assert_int_eq(frame.len, sizeof(data)); \
		ck_assert_mem_eq(frame.data, data, frame.len); \
	} while (false)
#define ckread_ack(COUNTER) ckread(true, COUNTER);
#define ckread_eof(ITEM) \
	do { \
		ck_assert_int_eq( \
			cp_unpack_type(rpcclient_unpack(canraw_c), ITEM), CPITEM_INVALID); \
		ck_assert_int_eq((ITEM)->as.Error, CPERR_EOF); \
		ck_assert(rpcclient_validmsg(canraw_c)); \
	} while (false)

#define ckwrite(PRIO, ...) \
	do { \
		struct canfd_frame frame = { \
			.can_id = 0x604 | (PRIO ? 0 : 0x100), \
			.flags = CANFD_BRS, \
			.len = sizeof((uint8_t[]){9, __VA_ARGS__}), \
			.data = {9, __VA_ARGS__}, \
		}; \
		ck_assert_int_eq(write(canraw_fd, &frame, CANFD_MTU), CANFD_MTU); \
	} while (false)
#define ckwrite_ack(COUNTER) ckwrite(true, COUNTER);

TEST(canraw, reset) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x84, 0);
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_RESET);
	ckread_ack(0x84);
}

TEST(canraw, disconnect) {
	if (canraw_fd == -1)
		return;
	ckwrite(false);
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_ERROR);
	ck_assert_int_eq(rpcclient_errno(canraw_c), ENOTCONN);
}

TEST(canraw, single_frame) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x85, 1, 0x86, 5, 'H', 'e', 'l', 'l', 'o');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x85);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "Hello");
	free(str);
	ckread_eof(&item);
}

TEST(canraw, multi_frame) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x08, 1, 0x86, 0x81, 0xbd, 'L', 'o', 'r', 'e', 'm', ' ', 'i',
		'p', 's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't',
		' ', 'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't',
		'e', 't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n',
		'g', ' ', 'e', 'l', 'i', 't', ',', ' ', 's');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x08);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	ck_assert_int_eq(item.as.String.eoff, 445);
	ckwrite(true, 0x09, 'e', 'd', ' ', 'd', 'o', ' ', 'e', 'i', 'u', 's', 'm',
		'o', 'd', ' ', 't', 'e', 'm', 'p', 'o', 'r', ' ', 'i', 'n', 'c', 'i',
		'd', 'i', 'd', 'u', 'n', 't', ' ', 'u', 't', ' ', 'l', 'a', 'b', 'o',
		'r', 'e', ' ', 'e', 't', ' ', 'd', 'o', 'l', 'o', 'r', 'e', ' ', 'm',
		'a', 'g', 'n', 'a', ' ', 'a', 'l', 'i', 'q');
	ckwrite(true, 0x0a, 'u', 'a', '.', ' ', 'U', 't', ' ', 'e', 'n', 'i', 'm',
		' ', 'a', 'd', ' ', 'm', 'i', 'n', 'i', 'm', ' ', 'v', 'e', 'n', 'i',
		'a', 'm', ',', ' ', 'q', 'u', 'i', 's', ' ', 'n', 'o', 's', 't', 'r',
		'u', 'd', ' ', 'e', 'x', 'e', 'r', 'c', 'i', 't', 'a', 't', 'i', 'o',
		'n', ' ', 'u', 'l', 'l', 'a', 'm', 'c', 'o');
	ckwrite(true, 0x0b, ' ', 'l', 'a', 'b', 'o', 'r', 'i', 's', ' ', 'n', 'i',
		's', 'i', ' ', 'u', 't', ' ', 'a', 'l', 'i', 'q', 'u', 'i', 'p', ' ',
		'e', 'x', ' ', 'e', 'a', ' ', 'c', 'o', 'm', 'm', 'o', 'd', 'o', ' ',
		'c', 'o', 'n', 's', 'e', 'q', 'u', 'a', 't', '.', ' ', 'D', 'u', 'i',
		's', ' ', 'a', 'u', 't', 'e', ' ', 'i', 'r');
	ckwrite(true, 0xc, 'u', 'r', 'e', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 'i',
		'n', ' ', 'r', 'e', 'p', 'r', 'e', 'h', 'e', 'n', 'd', 'e', 'r', 'i',
		't', ' ', 'i', 'n', ' ', 'v', 'o', 'l', 'u', 'p', 't', 'a', 't', 'e',
		' ', 'v', 'e', 'l', 'i', 't', ' ', 'e', 's', 's', 'e', ' ', 'c', 'i',
		'l', 'l', 'u', 'm', ' ', 'd', 'o', 'l', 'o');
	ckwrite(true, 0xd, 'r', 'e', ' ', 'e', 'u', ' ', 'f', 'u', 'g', 'i', 'a',
		't', ' ', 'n', 'u', 'l', 'l', 'a', ' ', 'p', 'a', 'r', 'i', 'a', 't',
		'u', 'r', '.', ' ', 'E', 'x', 'c', 'e', 'p', 't', 'e', 'u', 'r', ' ',
		's', 'i', 'n', 't', ' ', 'o', 'c', 'c', 'a', 'e', 'c', 'a', 't', ' ',
		'c', 'u', 'p', 'i', 'd', 'a', 't', 'a', 't');
	ckwrite(true, 0xe, ' ', 'n', 'o', 'n', ' ', 'p', 'r', 'o', 'i', 'd', 'e',
		'n', 't', ',', ' ', 's', 'u', 'n', 't', ' ', 'i', 'n', ' ', 'c', 'u',
		'l', 'p', 'a', ' ', 'q', 'u', 'i', ' ', 'o', 'f', 'f', 'i', 'c', 'i',
		'a', ' ', 'd', 'e', 's', 'e', 'r', 'u', 'n', 't', ' ', 'm', 'o', 'l',
		'l', 'i', 't', ' ', 'a', 'n', 'i', 'm', ' ');
	ckwrite(true, 0x8f, 'i', 'd', ' ', 'e', 's', 't', ' ', 'l', 'a', 'b', 'o',
		'r', 'u', 'm', '.');
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ckread_eof(&item);
	ck_assert_pstr_eq(str,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
	free(str);
}

TEST(canraw, multi_frame_sequence) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x08, 1, 0x86, 0x7b, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x08);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	ck_assert_int_eq(item.as.String.eoff, 123);
	char *str = cp_unpack_strndup(rpcclient_unpack(canraw_c), &item, 59);
	ck_assert_pstr_eq(
		str, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, se");
	free(str);
	ckwrite(true, 0x09, 'd', ' ', 'd', 'o', ' ', 'e', 'i', 'u', 's', 'm', 'o',
		'd', ' ', 't', 'e', 'm', 'p', 'o', 'r', ' ', 'i', 'n', 'c', 'i', 'd',
		'i', 'd', 'u', 'n', 't', ' ', 'u', 't', ' ', 'l', 'a', 'b', 'o', 'r',
		'e', ' ', 'e', 't', ' ', 'd', 'o', 'l', 'o', 'r', 'e', ' ', 'm', 'a',
		'g', 'n', 'a', ' ', 'a', 'l', 'i', 'q', 'u');
	str = cp_unpack_strndup(rpcclient_unpack(canraw_c), &item, 62);
	ck_assert_pstr_eq(
		str, "d do eiusmod tempor incididunt ut labore et dolore magna aliqu");
	free(str);
	ckwrite(true, 0x8a, 'a', '.');
	str = cp_unpack_strndup(rpcclient_unpack(canraw_c), &item, 62);
	ck_assert_pstr_eq(str, "a.");
	free(str);
	ckread_eof(&item);
}

/* Send multi-frame message followed by another first frame */
TEST(canraw, doublemsg) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x0a);
	ckwrite(true, 0x8b, 'd');
	ckwrite(false, 0x8c, 1, 0x86, 0x05, 'H', 'e', 'l', 'l', 'o');

	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	ck_assert_int_eq(item.as.String.eoff, 60);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ckread_eof(&item);
	ck_assert_pstr_eq(
		str, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed");
	free(str);

	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x8c);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "Hello");
	free(str);
	ckread_eof(&item);
}

/* Receive two messages where first is only validated (data has to be skipped)
 * and only the second one is fully read.
 */
TEST(canraw, unreadmsg) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x0a);
	ckwrite(true, 0x8b, 'd');
	ckwrite(false, 0x8c, 1, 0x86, 0x05, 'H', 'e', 'l', 'l', 'o');
	ck_assert(rpcclient_validmsg(canraw_c));
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x8c);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "Hello");
	free(str);
	ckread_eof(&item);
}

/* Receive two messages where first one is ignored and the second one is fully
 * read to check it.
 */
TEST(canraw, ignoremsg) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x0a);
	ckwrite(true, 0x8b, 'd');
	ckwrite(false, 0x8c, 1, 0x86, 0x05, 'H', 'e', 'l', 'l', 'o');
	rpcclient_ignoremsg(canraw_c);
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x8c);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "Hello");
	free(str);
	ckread_eof(&item);
}

/* CAN bus can send some messages multiple times if they are detected to be as
 * lost. Thus we have to ensure that at any point of receiveing the message we
 * are able to handle retransmits of those messages. What is ensured is that
 * retransmit is possible only for the latest frame only.
 */
TEST(canraw, retransmit) {
	if (canraw_fd == -1)
		return;
	for (int i = 0; i < 3; i++)
		ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i',
			'p', 's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i',
			't', ' ', 'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e',
			'c', 't', 'e', 't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's',
			'c', 'i', 'n', 'g', ' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x0a);
	for (int i = 0; i < 3; i++)
		ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i',
			'p', 's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i',
			't', ' ', 'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e',
			'c', 't', 'e', 't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's',
			'c', 'i', 'n', 'g', ' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	for (int i = 0; i < 3; i++)
		ckwrite(true, 0x8b, 'd');
	ckwrite(false, 0x8b, 1, 0x86, 5, 'H', 'e', 'l', 'l', 'o');
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(
		str, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed");
	free(str);
	ckread_eof(&item);

	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x8b);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "Hello");
	free(str);
	ckread_eof(&item);
}

/* Once the acknowledge is sent then the message must be kept. */
TEST(canraw, noreplacefirst) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x85, 1, 0x86, 5, 'H', 'e', 'l', 'l', 'o');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckwrite(false, 0x80, 1, 0x86, 5, 'W', 'o', 'r', 'l', 'd');
	ckread_ack(0x85);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "Hello");
	free(str);
	ckread_eof(&item);

	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x80);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "World");
	free(str);
	ckread_eof(&item);
}

/* The message must be aborted if not fully received. */
TEST(canraw, msgreset) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x0a);
	ckwrite(false, 0x80, 1, 0x86, 5, 'H', 'e', 'l', 'l', 'o');
	ck_assert(!rpcclient_validmsg(canraw_c));
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x80);
	ck_assert(rpcclient_validmsg(canraw_c));
}

/* Message abort due to lost frames */
TEST(canraw, lostframe) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x08, 1, 0x86, 0x7b, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x08);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char str[21] = {};
	ck_assert_int_eq(
		cp_unpack_strncpy(rpcclient_unpack(canraw_c), &item, str, 20), 20);
	ck_assert_pstr_eq(str, "Lorem ipsum dolor si");
	ckwrite(true, 0x8a, 'a', '.');
	ckwrite(false, 0x8c, 1, 0x86, 0x05, 'H', 'e', 'l', 'l', 'o');
	ck_assert_ptr_null(cp_unpack_strdup(rpcclient_unpack(canraw_c), &item));
	ck_assert_int_eq(item.type, CPITEM_INVALID);
	ck_assert_int_eq(item.as.Error, CPERR_IO);
	rpcclient_ignoremsg(canraw_c);

	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x8c);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	ck_assert_int_eq(
		cp_unpack_strncpy(rpcclient_unpack(canraw_c), &item, str, 20), 5);
	ckread_eof(&item);
}

/* The first frame if not accepted should be replaced. */
TEST(canraw, replacefirst) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x8a, 1, 0x86, 5, 'H', 'e', 'l', 'l', 'o');
	ckwrite(false, 0x80, 1, 0x86, 5, 'H', 'e', 'l', 'l', 'o');
	ckwrite(false, 0x80, 1, 0x86, 5, 'W', 'o', 'r', 'l', 'd');
	usleep(10000); /* Give reader time to consume all of the frames */
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x80);
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ck_assert_pstr_eq(str, "World");
	free(str);
	ckread_eof(&item);
}

TEST(canraw, replacefirstnotlast) {
	if (canraw_fd == -1)
		return;
	ckwrite(false, 0x0a, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ckwrite(false, 0x00, 1, 0x86, 0x3c, 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	ckwrite(false, 0x00, 1, 0x86, 0x3c, 'l', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
		's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ',
		'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e',
		't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g',
		' ', 'e', 'l', 'i', 't', ',', ' ', 's', 'e');
	usleep(10000); /* Give reader time to consume all of the frames */
	ck_assert_int_eq(rpcclient_nextmsg(canraw_c), RPCC_MESSAGE);
	ckread_ack(0x00);
	ckwrite(true, 0x81, 'd');
	struct cpitem item;
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(canraw_c), &item), CPITEM_STRING);
	char *str = cp_unpack_strdup(rpcclient_unpack(canraw_c), &item);
	ckread_eof(&item);
	ck_assert_pstr_eq(
		str, "lorem ipsum dolor sit amet, consectetur adipiscing elit, sed");
	free(str);
}
