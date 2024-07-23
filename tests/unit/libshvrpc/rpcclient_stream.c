#include <shv/rpcclient_stream.h>
#include <shv/rpcmsg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>

#define SUITE "rpcclient_stream"
#define DEFAULT_TEARDOWN teardown_client
#include <check_suite.h>
#include "../utils/bdata.h"

static rpcclient_t client = NULL;
static pthread_t pipethread;
static struct bdata clientres = {.v = NULL, .len = 0};


struct thrdloop_arg {
	const struct bdata *rdata;
	int pipes[2];
};
static void *thrdloop(void *_arg) {
	struct thrdloop_arg *arg = _arg;
	size_t woff = 0;
	size_t siz = 4, len = 0;
	uint8_t *data = malloc(siz);
	struct pollfd pfds[2] = {
		{.fd = arg->pipes[0], .events = POLLIN | POLLHUP},
		{.fd = arg->pipes[1], .events = 0},
	};
	if (arg->rdata && arg->rdata->len) {
		int i = write(pfds[1].fd, arg->rdata->v, arg->rdata->len);
		if (i <= 0)
			fprintf(stderr, "Write failed: %m\n");
		woff = i;
		if (woff < arg->rdata->len)
			pfds[1].events = POLLOUT;
	}
	while (pfds[0].events || pfds[1].events) {
		if (poll(pfds, 2, -1) <= 0)
			fprintf(stderr, "Poll failed: %m\n");
		if (pfds[0].revents) {
			uint8_t buf[BUFSIZ];
			int i = read(pfds[0].fd, buf, BUFSIZ);
			if (i > 0) {
				while (i >= (siz - len)) {
					siz *= 2;
					data = realloc(data, siz);
				}
				memcpy(data + len, buf, i);
				len += i;
			} else
				pfds[0].events = 0;
		}
		if (pfds[1].revents && arg->rdata) {
			int res =
				write(pfds[1].fd, arg->rdata->v + woff, arg->rdata->len - woff);
			if (res <= 0)
				pfds[1].events = 0;
			woff += res;
		}
	}
	close(arg->pipes[0]);
	close(arg->pipes[1]);
	free(arg);
	clientres.v = data;
	clientres.len = len;
	return NULL;
}

static void create_client(enum rpcstream_proto proto, const struct bdata *rdata) {
	struct thrdloop_arg *arg = malloc(sizeof *arg);
	client = rpcclient_pipe_new(arg->pipes, proto);
	ck_assert_ptr_nonnull(client);
	ck_assert_int_ge(arg->pipes[0], 0);
	ck_assert_int_ge(arg->pipes[1], 0);
	arg->rdata = rdata;
	ck_assert_int_eq(pthread_create(&pipethread, NULL, thrdloop, arg), 0);
	if (rdata) {
		/* Wait for the first loop */
		struct pollfd pfd = {
			.fd = rpcclient_pollfd(client), .events = POLLIN | POLLHUP};
		ck_assert_int_eq(poll(&pfd, 1, -1), 1);
	}
}

static void finalize_client(void) {
	rpcclient_disconnect(client);
	rpcclient_destroy(client);
	client = NULL;
	pthread_join(pipethread, NULL);
}

static void teardown_client(void) {
	if (client)
		finalize_client();
	free((uint8_t *)clientres.v);
}

#define ck_assert_clientres(DATA) \
	do { \
		finalize_client(); \
		ck_assert_bdata(clientres.v, clientres.len, (DATA)); \
	} while (false)


TEST_CASE(block) {}

static const struct bdata block_3msgs =
	B(0x02, 0x01, 0x6a, 0x02, 0x01, 0x6b, 0x02, 0x01, 0x06c);

TEST(block, block_receive) {
	create_client(RPCSTREAM_P_BLOCK, &block_3msgs);
	struct cpitem item;
	int val;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 42);
	ck_assert(!cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(item.as.Error, CPERR_EOF);
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	/* We skip here message reading and validation to ensure that we can handle
	 * unread messages.
	 */

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 44);
	ck_assert(!cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(item.as.Error, CPERR_EOF);
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_NOTHING);
}
END_TEST

static const char *const long_text =
	"This is some longer text to have message longer than 127 bytes...."
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor.";
static const struct bdata long_text_block_msg = B(0x80, 0x95, 0x01, 0x86, 0x80,
	0x91, 'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 's', 'o', 'm', 'e', ' ', 'l',
	'o', 'n', 'g', 'e', 'r', ' ', 't', 'e', 'x', 't', ' ', 't', 'o', ' ', 'h',
	'a', 'v', 'e', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e', ' ', 'l', 'o', 'n',
	'g', 'e', 'r', ' ', 't', 'h', 'a', 'n', ' ', '1', '2', '7', ' ', 'b', 'y',
	't', 'e', 's', '.', '.', '.', '.', 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p',
	's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's', 'i', 't', ' ', 'a',
	'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e', 't', 'u',
	'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g', ' ', 'e', 'l',
	'i', 't', ',', ' ', 's', 'e', 'd', ' ', 'd', 'o', ' ', 'e', 'i', 'u', 's',
	'm', 'o', 'd', ' ', 't', 'e', 'm', 'p', 'o', 'r', '.');

TEST(block, block_receive_long) {
	create_client(RPCSTREAM_P_BLOCK, &long_text_block_msg);
	struct cpitem item;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	char *str = cp_unpack_strdup(rpcclient_unpack(client), &item);
	ck_assert_str_eq(str, long_text);
	ck_assert(rpcclient_validmsg(client));
	free(str);

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_NOTHING);
}
END_TEST

static const struct bdata block_resetmsg = B(0x01, 0x00);

TEST(block, block_receive_reset) {
	create_client(RPCSTREAM_P_BLOCK, &block_resetmsg);
	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_RESET);
}
END_TEST

TEST(block, block_send) {
	create_client(RPCSTREAM_P_BLOCK, NULL);
	ck_assert(cp_pack_int(rpcclient_pack(client), 42));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert(cp_pack_int(rpcclient_pack(client), 43));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert_clientres(B(0x02, 0x01, 0x6a, 0x02, 0x01, 0x6b));
}
END_TEST

TEST(block, block_send_long) {
	create_client(RPCSTREAM_P_BLOCK, NULL);
	ck_assert(cp_pack_str(rpcclient_pack(client), long_text));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert_clientres(long_text_block_msg);
}
END_TEST

TEST(block, block_drop) {
	create_client(RPCSTREAM_P_BLOCK, NULL);
	ck_assert(cp_pack_str(rpcclient_pack(client), "This is packed to be dropped"));
	ck_assert(rpcclient_dropmsg(client));
	ck_assert_clientres(B());
}
END_TEST

TEST(block, block_send_reset) {
	create_client(RPCSTREAM_P_BLOCK, NULL);
	ck_assert(rpcclient_reset(client));
	ck_assert_clientres(B(0x01, 0x00));
}
END_TEST

TEST(block, block_pollfd) {
	create_client(RPCSTREAM_P_BLOCK, NULL);
	ck_assert_int_ne(rpcclient_pollfd(client), 0);
}
END_TEST

TEST(block, block_disconnect) {
	create_client(RPCSTREAM_P_BLOCK, NULL);
	ck_assert(rpcclient_connected(client));
	rpcclient_disconnect(client);
	ck_assert(!rpcclient_connected(client));
}


TEST_CASE(serial) {}

/* This data contains two bytes before message three messages to test that we
 * can handle unterminated messages.
 */
static const struct bdata serial_3msg = B(0x42, 0xa3, 0xa2, 0x01, 0x6a, 0xa3,
	0xa2, 0x01, 0x6b, 0xa3, 0xa2, 0x42, 0xa2, 0x01, 0x06c, 0xa3);

TEST(serial, serial_receive) {
	create_client(RPCSTREAM_P_SERIAL, &serial_3msg);
	struct cpitem item;
	int val;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 42);
	ck_assert(!cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(item.as.Error, CPERR_EOF);
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	/* We skip here message reading and validation to ensure that we can handle
	 * unread messages.
	 */
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 44);
	ck_assert(!cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(item.as.Error, CPERR_EOF);
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_NOTHING);
}
END_TEST

/* This checks that aborted message is invalid even if validly received. */
static const struct bdata serial_msgabort = B(0xa2, 0x01, 0x6a, 0xa4);
TEST(serial, serial_receive_abort) {
	create_client(RPCSTREAM_P_SERIAL, &serial_msgabort);
	struct cpitem item;
	int val;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 42);
	ck_assert(!rpcclient_validmsg(client));
}

/* This tests that all escaped bytes are correctly read. */
static const struct bdata serial_escmsg = B(0xa2, 0x01, 0x85, 0x04, 0xaa, 0x02,
	0xaa, 0x03, 0xaa, 0x04, 0xaa, 0x0a, 0xa3);
TEST(serial, serial_receive_escaped) {
	create_client(RPCSTREAM_P_SERIAL, &serial_escmsg);
	struct cpitem item;
	uint8_t *data;
	size_t siz;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(cp_unpack_type(rpcclient_unpack(client), &item), CPITEM_BLOB);
	cp_unpack_memdup(rpcclient_unpack(client), &item, &data, &siz);
	ck_assert_bdata(data, siz, B(0xa2, 0xa3, 0xa4, 0xaa));
	ck_assert(rpcclient_validmsg(client));
	free(data);
}

/* Check that we fail with invalid message if wrong escape is provided. */
static const struct bdata serial_invalidescmsg = B(0xa2, 0x01, 0xaa, 0x6a, 0xa3);
TEST(serial, serial_receive_invalid_escape) {
	create_client(RPCSTREAM_P_SERIAL, &serial_invalidescmsg);
	struct cpitem item;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(
		cp_unpack_type(rpcclient_unpack(client), &item), CPITEM_INVALID);
	ck_assert(!rpcclient_validmsg(client));
}

static const struct bdata serial_resetmsg = B(0xa2, 0x00, 0xa3);
TEST(serial, serial_receive_reset) {
	create_client(RPCSTREAM_P_SERIAL, &serial_resetmsg);
	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_RESET);
}
END_TEST

TEST(serial, serial_send) {
	create_client(RPCSTREAM_P_SERIAL, NULL);
	ck_assert(cp_pack_int(rpcclient_pack(client), 42));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert(cp_pack_blob(
		rpcclient_pack(client), (const uint8_t[]){0xa2, 0xa3, 0xa4, 0xaa}, 4));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert_clientres(B(0xa2, 0x01, 0x6a, 0xa3, 0xa2, 0x01, 0x85, 0x04, 0xaa,
		0x02, 0xaa, 0x03, 0xaa, 0x04, 0xaa, 0x0a, 0xa3));
}
END_TEST

TEST(serial, serial_drop) {
	create_client(RPCSTREAM_P_SERIAL, NULL);
	ck_assert(cp_pack_int(rpcclient_pack(client), 42));
	ck_assert(rpcclient_dropmsg(client));
	ck_assert_clientres(B(0xa2, 0x01, 0x6a, 0xa4));
}
END_TEST

TEST(serial, serial_send_reset) {
	create_client(RPCSTREAM_P_SERIAL, NULL);
	ck_assert(rpcclient_reset(client));
	ck_assert_clientres(B(0xa2, 0x00, 0xa3));
}
END_TEST


TEST_CASE(serial_crc) {}

static const struct bdata serial_crc_3msg = B(0x42, 0xa3, 0xa2, 0x01, 0x6a, 0xa3,
	0xf5, 0xa5, 0xab, 0xf8, 0x42, 0xa2, 0x01, 0x6a, 0xa3, 0xf5, 0xa5, 0xab, 0xf8);
TEST(serial_crc, serial_crc_receive) {
	create_client(RPCSTREAM_P_SERIAL_CRC, &serial_crc_3msg);
	struct cpitem item;
	int val;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 42);
	ck_assert(!cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(item.as.Error, CPERR_EOF);
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	/* We skip here message reading and validation to ensure that we can handle
	 * unread messages.
	 */
	ck_assert(rpcclient_validmsg(client));

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_NOTHING);
}
END_TEST

static const struct {
	uint8_t b;
	const struct bdata data;
} serial_crc_esc_crc_d[] = {
	{0x9d, B(0xa2, 0x01, 0x85, 0x01, 0x9d, 0xa3, 0xe9, 0xaa, 0x03, 0xbf, 0xaa, 0x0a)},
	{0x05, B(0xa2, 0x01, 0x85, 0x01, 0x05, 0xa3, 0x17, 0x77, 0xaa, 0x04, 0xdc)},
	{0x0f, B(0xa2, 0x01, 0x85, 0x01, 0x0f, 0xa3, 0xf7, 0xaa, 0x02, 0x4d, 0xc2)},
};
LOOP_TEST(serial_crc, serial_crc_receive_escaped_crc, 0,
	sizeof serial_crc_esc_crc_d / sizeof *serial_crc_esc_crc_d) {
	create_client(RPCSTREAM_P_SERIAL_CRC, &serial_crc_esc_crc_d[_i].data);
	struct cpitem item;
	uint8_t *data;
	size_t siz;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert_int_eq(cp_unpack_type(rpcclient_unpack(client), &item), CPITEM_BLOB);
	cp_unpack_memdup(rpcclient_unpack(client), &item, &data, &siz);
	ck_assert_bdata(data, siz, B(serial_crc_esc_crc_d[_i].b));
	ck_assert(rpcclient_validmsg(client));
	free(data);
}
END_TEST

static const struct bdata serial_crc_invalidcrcmsg =
	B(0xa2, 0x01, 0x6a, 0xa3, 0xf5, 0xaa, 0x05, 0xab, 0xf8);
TEST(serial_crc, serial_crc_receive_invalid_crc_escape) {
	create_client(RPCSTREAM_P_SERIAL_CRC, &serial_crc_invalidcrcmsg);
	struct cpitem item;
	int val;

	ck_assert_int_eq(rpcclient_nextmsg(client), RPCC_MESSAGE);
	cpitem_unpack_init(&item);
	ck_assert(cp_unpack_int(rpcclient_unpack(client), &item, val));
	ck_assert_int_eq(val, 42);
	ck_assert(!rpcclient_validmsg(client));
}
END_TEST

TEST(serial_crc, serial_crc_send) {
	create_client(RPCSTREAM_P_SERIAL_CRC, NULL);
	ck_assert(cp_pack_int(rpcclient_pack(client), 42));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert(cp_pack_blob(
		rpcclient_pack(client), (const uint8_t[]){0xa2, 0xa3, 0xa4, 0xaa}, 4));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert_clientres(B(0xa2, 0x01, 0x6a, 0xa3, 0xf5, 0xa5, 0xab, 0xf8, 0xa2,
		0x01, 0x85, 0x04, 0xaa, 0x02, 0xaa, 0x03, 0xaa, 0x04, 0xaa, 0x0a, 0xa3,
		0x35, 0x1e, 0xb3, 0x90));
}
END_TEST

static const struct {
	uint8_t b;
	const struct bdata data;
} serial_crc_send_esc_crc_d[] = {
	{0x9d, B(0xa2, 0x01, 0x85, 0x01, 0x9d, 0xa3, 0xe9, 0xaa, 0x03, 0xbf, 0xaa, 0x0a)},
	{0x05, B(0xa2, 0x01, 0x85, 0x01, 0x05, 0xa3, 0x17, 0x77, 0xaa, 0x04, 0xdc)},
	{0x0f, B(0xa2, 0x01, 0x85, 0x01, 0x0f, 0xa3, 0xf7, 0xaa, 0x02, 0x4d, 0xc2)},
};
ARRAY_TEST(serial_crc, serial_crc_send_esc_crc) {
	create_client(RPCSTREAM_P_SERIAL_CRC, NULL);
	ck_assert(cp_pack_blob(rpcclient_pack(client), &_d.b, 1));
	ck_assert(rpcclient_sendmsg(client));
	ck_assert_clientres(_d.data);
}
END_TEST

TEST(serial_crc, serial_crc_drop) {
	create_client(RPCSTREAM_P_SERIAL_CRC, NULL);
	ck_assert(cp_pack_int(rpcclient_pack(client), 42));
	ck_assert(rpcclient_dropmsg(client));
	ck_assert_clientres(B(0xa2, 0x01, 0x6a, 0xa4));
}
END_TEST

TEST(serial_crc, serial_crc_send_reset) {
	create_client(RPCSTREAM_P_SERIAL_CRC, NULL);
	ck_assert(rpcclient_reset(client));
	ck_assert_clientres(B(0xa2, 0x00, 0xa3, 0xd2, 0x02, 0xef, 0x8d));
}
END_TEST
