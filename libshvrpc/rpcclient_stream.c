#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <shv/chainpack.h>
#include <shv/crc32.h>
#include <shv/rpcclient_stream.h>

#define STX (0xA2)
#define ETX (0xA3)
#define ATX (0xA4)
#define ESC (0xAA)
#define ESC_STX (0x02)
#define ESC_ETX (0x03)
#define ESC_ATX (0x04)
#define ESC_ESC (0x0A)
#define ESCX(X) ((X) & 0x0f)
#define DESCX(X) ((X) | 0xA0)
#define ESCAPABLE(X) ((X) == STX || (X) == ETX || (X) == ATX || (X) == ESC)
#define ESCAPED(X) \
	((X) == ESC_STX || (X) == ESC_ETX || (X) == ESC_ATX || (X) == ESC_ESC)

#define TIMEOUT_RD (5000)
#define TIMEOUT_WR (200)
#define TIMEOUT_WR_BLOCK \
	(1000) /* Try harder on block that can't be recovered */
// TODO possibly add two levels of message priority and increase write timeout
// for requests and responses and keep it low only for signals.

struct ctx {
	struct rpcclient pub;
	const struct rpcclient_stream_funcs *sclient;
	void *sclient_cookie;
	enum rpcstream_proto proto;

	union {
		struct {
			int rfd, wfd;
		};
		int fds[2];
	};
	_Atomic int errnum;
	FILE *fr, *fw;

	union {
		struct rpcclient_ctx_block {
			size_t rmsgoff;
			char *wbuf;
			size_t wbuflen, wbufsiz;
		} block;
		struct rpcclient_ctx_serial {
			// TODO possibly buffer serial for higher read performance
			enum {
				WMSG_NO,   /* Not writing any message */
				WMSG_ST,   /* Started writting message */
				WMSG_DATA, /* Writing message data */
			} wmsg;
			enum {
				RMSG_NO,   /* Not reading any message */
				RMSG_STX,  /* Read STX but nothing more */
				RMSG_DATA, /* Reading message data */
				RMSG_ETX,  /* Read ETX but nothing more */
			} rmsg;
			crc32_t rcrc, wcrc;
		} serial;
	};
};

static ssize_t xread(struct ctx *c, char *buf, size_t siz, int timeout) {
	struct pollfd pfd = {
		.fd = c->rfd,
		.events = POLLIN,
	};
	while (true) {
		int pollres = poll(&pfd, 1, timeout);
		if (pollres < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		} else if (pollres == 0)
			return -2;
		break;
	}

	ssize_t i;
	do {
		i = read(c->rfd, buf, siz);
	} while (i == -1 && errno == EINTR);
	switch (i) {
		case -1:
			/* This can't happen even on O_NONBLOCK because we use poll. */
			assert(errno != EAGAIN && errno != EWOULDBLOCK);
			c->errnum = errno;
			break;
		case 0:
			c->errnum = ENOTCONN;
			break;
	}
	return i;
}
static int xreadc(struct ctx *c, int timeout) {
	uint8_t v;
	ssize_t res = xread(c, (void *)&v, 1, timeout);
	if (res > 0)
		return v;
	if (res == 0)
		/* In general this should not happend because read would block but for
		 * testing purposes we use files with this and for them this is EOF and
		 * this technically a disconnect.
		 */
		return -1;
	return res;
}

/* Reliable variant of standard write */
static bool xwrite(struct ctx *c, const void *buf, size_t siz) {
	if (c->errnum != 0 && c->errnum != EAGAIN)
		return false;
	uint8_t *data = (uint8_t *)buf;
	struct pollfd pfd = {
		.fd = c->wfd,
		.events = POLLOUT,
	};
	while (siz > 0) {
		int pollres = poll(&pfd, 1,
			c->errnum
				? 0
				: (c->proto == RPCSTREAM_P_BLOCK ? TIMEOUT_WR_BLOCK : TIMEOUT_WR));
		if (pollres < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			c->errnum = errno;
			return false;
		} else if (pollres == 0) {
			c->errnum = EWOULDBLOCK;
			return false;
		} else if (c->errnum)
			c->errnum = 0;

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

/* Block **********************************************************************/

static ssize_t cookie_read_block(void *cookie, char *buf, size_t size) {
	struct ctx *c = (struct ctx *)cookie;
	if (c->block.rmsgoff < size)
		/* Read up to the end of the message and then signal EOF by reading 0. */
		size = c->block.rmsgoff;
	if (size == 0)
		return 0;
	ssize_t res = xread(c, buf, size, TIMEOUT_RD);
	if (res >= 0)
		c->block.rmsgoff -= res;
	if (res == -2)
		c->errnum = ETIMEDOUT;
	return res;
}

static ssize_t cookie_write_block(void *cookie, const char *buf, size_t size) {
	struct ctx *c = (struct ctx *)cookie;
	while (c->block.wbuflen + size >= c->block.wbufsiz) {
		c->block.wbufsiz = 2 * (c->block.wbufsiz ?: 4);
		char *nbuf = realloc(c->block.wbuf, c->block.wbufsiz);
		if (!nbuf)
			return 0;
		c->block.wbuf = nbuf;
	}
	memcpy(c->block.wbuf + c->block.wbuflen, buf, size);
	c->block.wbuflen += size;
	return size;
}

static bool rpcclient_stream_block_send(struct ctx *c) {
	if (c->block.wbuflen == 0)
		return false;
	/* Write size */
	unsigned bytes = chainpack_w_uint_bytes(c->block.wbuflen);
	for (unsigned i = 0; i < bytes; i++) {
		uint8_t v = i == 0 ? chainpack_w_uint_value1(c->block.wbuflen, bytes)
						   : 0xff & (c->block.wbuflen >> (8 * (bytes - i - 1)));
		if (!xwrite(c, &v, 1))
			return false;
	}
	/* Write buffer */
	bool res = xwrite(c, c->block.wbuf, c->block.wbuflen);
	/* Note: We could keep the message on failure here for resend but that would
	 * not be consistent with stream protocol abilities.
	 */
	c->block.wbuflen = 0;
	return res;
}

static bool rpcclient_stream_block_drop(struct ctx *c) {
	c->block.wbuflen = 0;
	// TODO possibly decrease size of the write buffer based on the
	// some sort of statistics
	return true;
}

static enum rpcclient_msg_type rpcclient_stream_block_nextmsg(struct ctx *c) {
	int v;
	if ((v = xreadc(c, 0)) < 0)
		return v == -2 ? RPCC_NOTHING : RPCC_ERROR;
	unsigned bytes = chainpack_int_bytes(v);
	size_t msgsiz = chainpack_uint_value1(v, bytes);
	for (unsigned i = 1; i < bytes; i++) {
		if ((v = xreadc(c, TIMEOUT_RD)) == -1) {
			c->errnum = ETIMEDOUT;
			return RPCC_ERROR;
		}
		msgsiz = (msgsiz << 8) | v;
	}
	if (msgsiz < 0) {
		return RPCC_ERROR; /* Invalid size, something went wrong */
	}
	c->block.rmsgoff = msgsiz;
	return RPCC_MESSAGE;
}

/* Serial *********************************************************************/

static ssize_t cookie_read_serial(void *cookie, char *buf, size_t size) {
	struct ctx *c = (struct ctx *)cookie;
	if (c->serial.rmsg != RMSG_DATA)
		return c->errnum ? -1 : 0;
	size_t i;
	for (i = 0; i < size; i++) {
		int v = xreadc(c, TIMEOUT_RD);
		switch (v) {
			case STX:
				c->serial.rmsg = RMSG_STX;
				return -1;
			case ETX:
				c->serial.rmsg = RMSG_ETX;
				return i;
			case ESC:
				c->serial.rcrc = crc32_cupdate(c->serial.rcrc, v);
				v = xreadc(c, TIMEOUT_RD);
				if (ESCAPED(v)) {
					c->serial.rcrc = crc32_cupdate(c->serial.rcrc, v);
					buf[i] = DESCX(v);
					break;
				}
				/* Fall through in case of error or unexpected byte */
			case ATX:
				c->serial.rmsg = RMSG_NO;
				return -1;
			default:
				if (v < 0) {
					c->serial.rmsg = RMSG_NO;
					return v;
				}
				c->serial.rcrc = crc32_cupdate(c->serial.rcrc, v);
				buf[i] = v;
				break;
		}
	}
	return i;
}

static ssize_t cookie_write_serial(void *cookie, const char *data, size_t size) {
	struct ctx *c = (struct ctx *)cookie;
	char buf[BUFSIZ];
	bool esc = false;
	size_t cnt = 0;
	while (cnt < size) {
		size_t i;
		for (i = 0; i < BUFSIZ && cnt < size; i++) {
			if (c->serial.wmsg == WMSG_DATA) {
				if (esc) {
					buf[i] = ESCX(*data++);
					cnt++;
					esc = false;
				} else {
					if (ESCAPABLE((uint8_t)*data)) {
						buf[i] = ESC;
						esc = true;
					} else {
						buf[i] = *data++;
						cnt++;
					}
				}
				c->serial.wcrc = crc32_cupdate(c->serial.wcrc, buf[i]);
			} else {
				buf[i] = STX;
				c->serial.wmsg = WMSG_DATA;
				c->serial.wcrc = crc32_init();
			}
		}
		if (!xwrite(c, buf, i))
			return cnt;
	}
	return size;
}

static bool rpcclient_stream_serial_send(struct ctx *c) {
	if (c->serial.wmsg != WMSG_DATA)
		return false;
	c->serial.wmsg = WMSG_NO;
	uint8_t buf[9]; /* ETX plus every crc byte can be escaped */
	size_t i = 1;
	buf[0] = ETX;
	if (c->proto == RPCSTREAM_P_SERIAL_CRC) {
		uint32_t crc = crc32_finalize(c->serial.wcrc);
		for (int y = 3; y >= 0; y--) {
			register uint8_t v = (crc >> (8 * y)) & 0xff;
			if (ESCAPABLE(v)) {
				buf[i++] = ESC;
				buf[i++] = ESCX(v);
			} else
				buf[i++] = v;
		}
	}
	if (!xwrite(c, buf, i))
		return false;
	return true;
}

static bool rpcclient_stream_serial_drop(struct ctx *c) {
	if (c->serial.wmsg == WMSG_NO)
		return true;
	c->serial.wmsg = WMSG_NO;
	uint8_t v = ATX;
	if (!xwrite(c, &v, 1))
		return false;
	return true;
}

static enum rpcclient_msg_type rpcclient_stream_serial_nextmsg(struct ctx *c) {
	if (c->serial.rmsg != RMSG_STX) {
		c->serial.rmsg = RMSG_NO; /* Sanitize if validate wasn't called */
		int v;
		do {
			v = xreadc(c, 0);
		} while (v >= 0 && v != STX);
		if (v != STX)
			return v == -2 ? RPCC_NOTHING : RPCC_ERROR;
	}
	c->serial.rcrc = crc32_init();
	c->serial.rmsg = RMSG_DATA;
	return RPCC_MESSAGE;
}

static bool rpcclient_stream_serial_validate(struct ctx *c) {
	if (c->serial.rmsg != RMSG_ETX)
		return false;
	if (c->proto != RPCSTREAM_P_SERIAL_CRC)
		return true;
	uint32_t crc = 0;
	for (int i = 0; i < 4; i++) {
		int v = xreadc(c, TIMEOUT_RD);
		if (v < 0)
			return false;
		if (v == ESC) {
			v = xreadc(c, TIMEOUT_RD);
			if (!ESCAPED(v))
				return false;
			v = DESCX(v);
		}
		crc = (crc << 8) | (v & 0xff);
	}
	return crc == crc32_finalize(c->serial.rcrc);
}

/******************************************************************************/

static bool stream_pack(void *ptr, const struct cpitem *item) {
	struct ctx *c = (struct ctx *)((char *)ptr - offsetof(struct ctx, pub.pack));
	rpclogger_log_item(c->pub.logger_out, item);
	if ((c->proto == RPCSTREAM_P_BLOCK && c->block.wbuflen == 0) ||
		(c->proto != RPCSTREAM_P_BLOCK && c->serial.wmsg == WMSG_NO)) {
		putc(1, c->fw); /* Chainpack identifier */
		if (c->proto != RPCSTREAM_P_BLOCK)
			c->serial.wmsg = WMSG_ST;
	}
	return chainpack_pack(c->fw, item) > 0;
}

static void stream_unpack(void *ptr, struct cpitem *item) {
	struct ctx *c = (struct ctx *)((char *)ptr - offsetof(struct ctx, pub.unpack));
	chainpack_unpack(c->fr, item);
	rpclogger_log_item(c->pub.logger_in, item);
}

static void flushmsg(struct ctx *c) {
	/* Flush the rest of the message. Read up to the EOF. */
	char buf[BUFSIZ];
	while (fread_unlocked(buf, 1, BUFSIZ, c->fr) > 0) {}
}

static void default_disconnect(int fd[2]) {
	if (fd[0] == -1)
		return;
	if (fd[0] != fd[1])
		close(fd[0]);
	close(fd[1]);
}

static int stream_ctrl(rpcclient_t client, enum rpcclient_ctrlop op) {
	struct ctx *c = (struct ctx *)client;
	switch (op) {
		case RPCC_CTRLOP_DESTROY:
			if (c->sclient->disconnect)
				c->sclient->disconnect(c->sclient_cookie, c->fds, true);
			else
				default_disconnect(c->fds);
			fclose(c->fr);
			fclose(c->fw);
			if (c->proto == RPCSTREAM_P_BLOCK)
				free(c->block.wbuf);
			free(c);
			return true;
		case RPCC_CTRLOP_DISCONNECT:
			if (c->rfd < 0)
				return false;
			if (c->sclient->disconnect)
				c->sclient->disconnect(c->sclient_cookie, c->fds, false);
			else
				default_disconnect(c->fds);
			c->rfd = -1;
			c->wfd = -1;
			return true;
		case RPCC_CTRLOP_RESET:
			if (c->rfd < 0) {
				bool sendrst = false;
				if (c->sclient->connect)
					sendrst = c->sclient->connect(c->sclient_cookie, c->fds);
				if (!sendrst || c->rfd < 0)
					return c->rfd >= 0;
			}
			putc(0, c->fw);
			rpclogger_log_reset(c->pub.logger_out);
			return stream_ctrl(client, RPCC_CTRLOP_SENDMSG);
		case RPCC_CTRLOP_ERRNO:
			if (c->rfd < 0)
				return ENOTCONN;
			return c->errnum;
		case RPCC_CTRLOP_NEXTMSG: {
			rpclogger_log_end(c->pub.logger_in, RPCLOGGER_ET_UNKNOWN);
			int ret;
			while (true) {
				if (c->proto == RPCSTREAM_P_BLOCK) {
					flushmsg(c);
					ret = rpcclient_stream_block_nextmsg(c);
				} else
					ret = rpcclient_stream_serial_nextmsg(c);
				if (ret != RPCC_MESSAGE)
					return ret;
				clearerr(c->fr);
				/* Read identifier byte */
				switch (getc_unlocked(c->fr)) {
					case 1: /* Chainpack message */
						return ret;
					case 0: /* Reset requested */
						rpclogger_log_reset(c->pub.logger_in);
						if (stream_ctrl(client, RPCC_CTRLOP_VALIDMSG))
							return RPCC_RESET;
						break;
					default: /* Ignore other message types */
						break;
				}
			}
		}
		case RPCC_CTRLOP_VALIDMSG: {
			flushmsg(c);
			bool res = c->proto == RPCSTREAM_P_BLOCK
				? true /* Block protocol doesn't have validation */
				: rpcclient_stream_serial_validate(c);
			rpclogger_log_end(c->pub.logger_in,
				res ? RPCLOGGER_ET_VALID : RPCLOGGER_ET_INVALID);
			return res;
		}
		case RPCC_CTRLOP_IGNOREMSG:
			flushmsg(c);
			if (c->proto == RPCSTREAM_P_BLOCK)
				rpcclient_stream_serial_validate(c);
			return 0;
		case RPCC_CTRLOP_SENDMSG: {
			bool res;
			if (c->proto == RPCSTREAM_P_BLOCK) {
				res = rpcclient_stream_block_send(c);
			} else {
				fflush(c->fw);
				res = rpcclient_stream_serial_send(c);
			}
			if (c->sclient->flush)
				c->sclient->flush(c->sclient_cookie, c->wfd);
			rpclogger_log_end(c->pub.logger_out, RPCLOGGER_ET_VALID);
			if (c->errnum == EWOULDBLOCK)
				/* Block protocol can't recover from partial message and thus
				 * mark its connection and timed out.
				 */
				c->errnum = c->proto == RPCSTREAM_P_BLOCK ? ETIMEDOUT : EAGAIN;
			return res;
		}
		case RPCC_CTRLOP_DROPMSG: {
			bool res;
			if (c->proto == RPCSTREAM_P_BLOCK) {
				res = rpcclient_stream_block_drop(c);
			} else {
				fflush(c->fw);
				res = rpcclient_stream_serial_drop(c);
			}

			// TODO this is pretty much same as send (merge the code)
			if (c->sclient->flush)
				c->sclient->flush(c->sclient_cookie, c->wfd);
			rpclogger_log_end(c->pub.logger_out, RPCLOGGER_ET_INVALID);
			if (c->errnum == EWOULDBLOCK)
				c->errnum = EAGAIN;

			return res;
		}
		case RPCC_CTRLOP_CONTRACK:
			return c->sclient->contrack;
		case RPCC_CTRLOP_POLLFD:
			return c->rfd;
	}
	/* This should not happen -> implementation error */
	abort(); // GCOVR_EXCL_LINE
}

static size_t stream_peername(rpcclient_t client, char *buf, size_t siz) {
	struct ctx *c = (struct ctx *)client;
	if (c->sclient->peername)
		return c->sclient->peername(c->sclient_cookie, c->fds, buf, siz);
	if (buf && siz > 0)
		*buf = '\0';
	return 0;
}

rpcclient_t rpcclient_stream_new(const struct rpcclient_stream_funcs *sclient,
	void *sclient_cookie, enum rpcstream_proto proto, int rfd, int wfd) {
	struct ctx *res = malloc(sizeof *res);
	*res = (struct ctx){
		.pub =
			(struct rpcclient){
				.ctrl = stream_ctrl,
				.peername = stream_peername,
				.pack = stream_pack,
				.unpack = stream_unpack,
			},
		.sclient = sclient,
		.sclient_cookie = sclient_cookie,
		.proto = proto,
		.rfd = rfd,
		.wfd = wfd,
		.errnum = 0,
		.fr = fopencookie(res, "r+",
			(cookie_io_functions_t){
				.read = proto == RPCSTREAM_P_BLOCK ? cookie_read_block
												   : cookie_read_serial,
			}),
		.fw = fopencookie(res, "r+",
			(cookie_io_functions_t){
				.write = proto == RPCSTREAM_P_BLOCK ? cookie_write_block
													: cookie_write_serial,
			}),
	};
	setbuf(res->fr, NULL);
	if (proto == RPCSTREAM_P_BLOCK)
		setbuf(res->fw, NULL);
	return &res->pub;
}
