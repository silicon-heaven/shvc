#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
#include <shv/rpctransport.h>
#include "shvc_config.h"

#ifndef SHVC_CAN

rpcclient_t rpcclient_can_new(
	const char *interface, uint8_t address, uint8_t local_address) {
	return NULL;
}

rpcserver_t rpcserver_can_new(const char *interface, uint8_t address) {
	return NULL;
}

#else

# ifdef SHVC_NUTTXCAN
#  include <nuttx/can/can.h>
#  include <nuttx/config.h>
#  define canid(F) (F).cm_hdr.ch_id
#  define candata(F) (F).cm_data
#  ifdef CONFIG_CAN_EXTID
#   define canexid(F) (F).cm_hdr.ch_extid
#  else
#   define canexid(F) (false)
#  endif
#  ifdef CONFIG_CAN_ERRORS
#   define canerr(F) (F).cm_hdr.ch_error
#  else
#   define canerr(F) (false)
#  endif
#  define canrtr(F) (F).cm_hdr.ch_rtr
#  define wframe_len(C) (C)->wlen
typedef struct can_msg_s frame_t;
# else
#  include <linux/can/raw.h>
#  include <linux/if.h>
#  define canid(F) (F).can_id
#  define candata(F) (F).data
#  define canexid(F) ((F).can_id & CAN_EFF_FLAG)
#  define canerr(F) ((F).can_id & CAN_ERR_FLAG)
#  define canrtr(F) ((F).can_id & CAN_RTR_FLAG)
#  define wframe_len(C) (C)->wframe.len
typedef struct canfd_frame frame_t;
# endif

# define CANID_SHVCAN 0x400
# define CANID_UNUSED 0x200
# define CANID_FIRST_MASK 0x100
# define CANID_ADDRESS_MASK 0x0FF

struct ctx;

struct local {
	char *interface;
	uint8_t address;
	int fd, listenfd;
	pthread_t thread;
	_Atomic int errnum;

	pthread_mutex_t mtx;
	struct ctx **ctxs;
	size_t ctxscnt;
};

struct ctx {
	struct rpcclient pub;
	struct local *local;
	const char *interface;
	uint8_t address, local_address;
	pthread_mutex_t mtx;

	int reventfd;
	uint8_t rcounter;
	uint8_t rdata[62];
	uint8_t rsiz, roff;
	enum rstate {
		RS_NONE,  /* No message is being received or it is being ignored. */
		RS_FIRST, /* First frame received but not acknowledged yet. */
		RS_MSG, /* First frame was acknowledged and message is being received. */
		RS_ABORT,	   /* Signal abort of receaving message. */
		RS_DISCONNECT, /* Disconnect was signalled. */
	} rstate;
	bool rtoken;
	pthread_cond_t rcond, rrcond;

	frame_t wframe;
# ifdef SHVC_NUTTXCAN
	uint8_t wlen;
# endif
	bool wack;
	uint8_t wackb;
	pthread_cond_t wackcond;

	FILE *fr, *fw;
};

static struct ctx *ctx_new(
	uint8_t address, uint8_t local_address, const char *reset_interface);
static bool ctx_register(struct local *local, struct ctx *ctx);
static void ctx_free(struct ctx *ctx);


static struct _ctx {
	size_t i;
	struct ctx *ctx;
} _getctx(struct local *local, uint8_t address) {
	struct _ctx res = {};
	for (size_t l = 0, h = local->ctxscnt; l != h;) {
		res.i = l + (h - l) / 2;
		if (local->ctxs[res.i]->address < address) {
			l = res.i + 1;
		} else if (local->ctxs[res.i]->address > address) {
			h = res.i;
		} else {
			res.ctx = local->ctxs[res.i];
			break;
		}
	}
	return res;
}
static inline struct ctx *getctx(struct local *local, uint8_t address) {
	struct _ctx res = _getctx(local, address);
	return res.ctx;
}

static bool send_ack(struct ctx *ctx, uint8_t counter) {
	assert(ctx->local);
# ifdef SHVC_NUTTXCAN
	frame_t frame = {
		.cm_hdr.ch_id = ctx->local_address | CANID_SHVCAN | CANID_UNUSED,
		.cm_hdr.ch_dlc = 2,
		.cm_hdr.ch_edl = 1,
		.cm_hdr.ch_brs = 1,
		.cm_data[0] = ctx->address,
		.cm_data[1] = counter,
	};
	size_t siz = CAN_MSGLEN(frame.cm_hdr.ch_dlc);
# else
	frame_t frame = {
		.can_id = ctx->local_address | CANID_SHVCAN | CANID_UNUSED,
		.flags = CANFD_BRS,
		.len = 2,
		.data[0] = ctx->address,
		.data[1] = counter,
	};
	const size_t siz = sizeof frame;
# endif
	return write(ctx->local->fd, &frame, siz) == siz;
};

static bool send_data_frame(struct ctx *c) {
	pthread_mutex_lock(&c->mtx);
	while (!c->wack) {
		struct timespec at;
		assert(clock_gettime(CLOCK_REALTIME, &at) == 0);
		at.tv_sec += 1;
		if (pthread_cond_timedwait(&c->wackcond, &c->mtx, &at) == ETIMEDOUT) {
			pthread_mutex_unlock(&c->mtx);
			return false;
		}
	}
# ifdef SHVC_NUTTXCAN
	c->wframe.cm_hdr.ch_dlc = can_bytes2dlc(c->wlen);
	const size_t siz = CAN_MSGLEN(can_dlc2bytes(c->wframe.cm_hdr.ch_dlc));
# else
	const size_t siz = sizeof(frame_t);
# endif
	ssize_t i = write(c->local->fd, &c->wframe, siz);
	if (i != siz) {
		pthread_mutex_unlock(&c->mtx);
		return false;
	}
	if (canid(c->wframe) & CANID_FIRST_MASK) {
		c->wack = false;
		c->wackb = candata(c->wframe)[1];
	}
	pthread_mutex_unlock(&c->mtx);
	candata(c->wframe)[1] = (candata(c->wframe)[1] + 1) & 0x7f;
	canid(c->wframe) = 0;
	return true;
}

static bool send_rtr_frame(struct local *local, uint8_t dlc) {
	assert(dlc <= 8); /* Not supported for now. Requires decode to len. */
# ifdef SHVC_NUTTXCAN
	struct can_msg_s frame = {
		.cm_hdr.ch_id = local->address | CANID_SHVCAN | CANID_UNUSED,
		.cm_hdr.ch_rtr = 1,
		.cm_hdr.ch_dlc = dlc,
	};
	const size_t siz = CAN_MSGLEN(0);
# else
	struct can_frame frame = {
		.can_id = CAN_RTR_FLAG | local->address | CANID_SHVCAN | CANID_UNUSED,
		.len = dlc,
	};
	const size_t siz = sizeof frame;
# endif
	bool res = write(local->fd, &frame, siz) == siz;
	return res;
}


static void *_reader(void *arg) {
	struct local *local = arg;

	sigset_t sigset;
	sigfillset(&sigset);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

# define RTOKEN_PASS \
	 do { \
		 ctx->rtoken = true; \
		 pthread_cond_signal(&ctx->rcond); \
	 } while (false)
# define RTOKEN_WAIT \
	 while (ctx->rtoken) \
		 pthread_cond_wait(&ctx->rrcond, &ctx->mtx);
# define REVENTFD_SET \
	 do { \
		 uint64_t _ = 1; \
		 assert(write(ctx->reventfd, &_, sizeof _) == sizeof _); \
	 } while (false)

	while (true) {
		frame_t frame;
		size_t n = read(local->fd, &frame, sizeof frame);
# ifdef SHVC_NUTTXCAN
		size_t len = can_dlc2bytes(frame.cm_hdr.ch_dlc);
		if (n < CAN_MSGLEN(0) || n > sizeof frame) {
# else
		size_t len = frame.len;
		if (n != CANFD_MTU && n != CAN_MTU) {
# endif
			// TODO should we somehow terminate local object?
			syslog(LOG_ERR, "CAN frame read error: %s", strerror(errno));
			return NULL;
		}

		if (!(canid(frame) & CANID_SHVCAN) || canexid(frame) || canerr(frame))
			continue;

		uint8_t src = canid(frame) & CANID_ADDRESS_MASK;
		bool first = canid(frame) & CANID_FIRST_MASK;

		if (canrtr(frame)) {
			switch (len) {
				case 0x00:
					// TODO support dynamic address range
					break;
				case 0x1:
				case 0x2:
					// TODO address discovery (not probably required on shvc)
					break;
				case 0x5: /* Address discovery for listening peers */
					if (local->listenfd >= 0)
						send_rtr_frame(local, 0x1); /* Allowed to fail */
					break;
				case 0x6: /* Address discovery for not-listening peers */
					if (local->listenfd < 0)
						send_rtr_frame(local, 0x2); /* Allowed to fail */
					break;
				case 0x7: /* General address discovery */
					send_rtr_frame(local, local->listenfd < 0 ? 0x2 : 0x1);
					break;
			}
			continue;
		}

		if (len < 1)
			continue; /* Invalid frame (at least destination must be present) */
		uint8_t dest = candata(frame)[0];
		if (dest != local->address)
			continue; /* Not for us. */
		pthread_mutex_lock(&local->mtx);
		struct ctx *ctx = getctx(local, src);
		if (ctx == NULL && len > 2 && first && local->listenfd >= 0) {
			/* New peer connected */
			ctx = ctx_new(src, dest, NULL);
			if (ctx) {
				if (ctx_register(local, ctx)) {
					assert(write(local->listenfd, &ctx, sizeof ctx) == sizeof ctx);
					if (len == 3 && candata(frame)[2] == 0) {
						/* Ignore initial message if it is reset */
						send_ack(ctx, candata(frame)[1]);
						ctx = NULL; /* Now we are done with this message */
					}
				} else {
					ctx_free(ctx);
					ctx = NULL;
				}
			}
		}
		if (ctx != NULL && ctx->rstate != RS_DISCONNECT) {
			if (len == 1) { /* Disconnect */
				pthread_mutex_lock(&ctx->mtx);
				RTOKEN_WAIT;
				ctx->rstate = RS_DISCONNECT;
				RTOKEN_PASS;
				REVENTFD_SET;
				pthread_mutex_unlock(&ctx->mtx);

			} else if (len == 2) { /* Acknowledgement */
				pthread_mutex_lock(&ctx->mtx);
				if (candata(frame)[1] == ctx->wackb) {
					ctx->wack = true;
					pthread_cond_signal(&ctx->wackcond);
				}
				pthread_mutex_unlock(&ctx->mtx);

			} else { /* Data */
				bool last = candata(frame)[1] & 0x80;
				if (last && len > 8) /* Crop the message length */
					while (candata(frame)[len - 1] == 0)
						len -= 1;
				pthread_mutex_lock(&ctx->mtx);
				/* We ignore here retransmit as that is allowed to happen. */
				bool retransmit = ctx->rcounter == candata(frame)[1] &&
					(!first || !memcmp(ctx->rdata, candata(frame) + 2, len - 2));
				if (!retransmit && (first || ctx->rstate == RS_MSG)) {
					bool seq = ((ctx->rcounter + 1) % 0x7f) ==
						(candata(frame)[1] & 0x7f);
					if (first || seq) {
						if (first) {
							if (ctx->rstate != RS_NONE && ctx->rstate != RS_FIRST) {
								if (!(ctx->rcounter & 0x80)) {
									/* The currently handled message is to be
									 * aborted if it is not the last one. */
									ctx->rstate = RS_ABORT;
									RTOKEN_PASS;
								}
								RTOKEN_WAIT;
							} /* Else replace the message. */
						} else
							RTOKEN_WAIT;
						/* Note: State might have changed while we were waiting
						 * for the token.
						 */
						if (first || ctx->rstate == RS_MSG) {
							memcpy(ctx->rdata, candata(frame) + 2, len - 2);
							ctx->rcounter = candata(frame)[1];
							ctx->rsiz = len - 2;
							ctx->roff = 0;
							ctx->rstate = first ? RS_FIRST : RS_MSG;
							RTOKEN_PASS;
							if (first) {
								REVENTFD_SET;
							}
						}
					} else if (ctx->rstate == RS_MSG) {
						/* No need to wait for the token. We set state to abort
						 * and opportunistically pass the token. Even if rstate
						 * is not checked after rtoken is passed it will be
						 * propagated later on. Contrary to the abort
						 * propagation in this reader thread.
						 */
						ctx->rstate = RS_ABORT;
						RTOKEN_PASS;
					}
				}
				pthread_mutex_unlock(&ctx->mtx);
			}
		}
		pthread_mutex_unlock(&local->mtx);
	}

# undef RTOKEN_WAIT
# undef RTOKEN_PASS
}

# define RTOKEN_PASS \
	 do { \
		 c->rtoken = false; \
		 pthread_cond_signal(&c->rrcond); \
	 } while (false)
/* TODO timeout should be 5 but that is too long for this implementation. */
# define RTOKEN_WAIT(TIMEOUT) \
	 ({ \
	  bool res = true; \
	  while (!c->rtoken && res) { \
		  struct timespec at; \
		  if (TIMEOUT) { \
			  assert(clock_gettime(CLOCK_REALTIME, &at) == 0); \
			  at.tv_sec += 1; \
		  } else \
			  at = (struct timespec){}; \
		  res = pthread_cond_timedwait(&c->rcond, &c->mtx, &at) != ETIMEDOUT; \
	  } \
	  res; \
	 })


static bool local_init(
	struct local *local, const char *interface, uint8_t address, int listenfd) {
# ifdef SHVC_NUTTXCAN
	char *devpath;
	asprintf(&devpath, "/dev/%s", interface);
	int fd = open(devpath, O_RDWR);
	free(devpath);
	if (fd < 0)
		return false;
	assert(ioctl(fd, CANIOC_SET_MSGALIGN, &(unsigned){0}) == 0);
# else
	int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (fd == -1)
		return false;
	// TODO possibly filter out extended ID messages and those without set SHV
	int val = 1;
	if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &val, sizeof val) == -1) {
		close(fd);
		return false;
	}
	struct ifreq ifr;
	strncpy(ifr.ifr_name, interface, sizeof ifr.ifr_name);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
		close(fd);
		return false;
	}
	struct sockaddr_can addr = {
		.can_family = AF_CAN, .can_ifindex = ifr.ifr_ifindex};
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
		close(fd);
		return false;
	}
# endif

	*local = (struct local){
		.interface = strdup(interface),
		.fd = fd,
		.listenfd = listenfd,
		.mtx = PTHREAD_MUTEX_INITIALIZER,
		.address = address,
		.ctxs = NULL,
		.ctxscnt = 0,
	};
	if (listenfd >= 0)			  /* Announce listen */
		send_rtr_frame(local, 1); /* Allowed to fail */
	return pthread_create(&local->thread, NULL, _reader, local) == 0;
}

static void local_destroy(struct local *local) {
	assert(local);
	assert(local->ctxscnt == 0);
	if (local->listenfd >= 0)
		close(local->listenfd);
	pthread_cancel(local->thread);
	pthread_join(local->thread, NULL);
	close(local->fd);
	pthread_mutex_destroy(&local->mtx);
	free(local->interface);
	/* local->ctxs is NULL, see ctx_unregister and assert at the start of this
	 * function to understand why.
	 */
}

static bool ctx_register(struct local *local, struct ctx *ctx) {
	assert(ctx->local == NULL);
	assert(local->address == ctx->local_address);
	struct _ctx c = _getctx(local, ctx->address);
	if (c.ctx)
		return false;

	struct ctx **newctxs =
		realloc(local->ctxs, (local->ctxscnt + 1) * sizeof *local->ctxs);
	if (newctxs == NULL)
		return false;
	local->ctxs = newctxs;
	memmove(local->ctxs + c.i + 1, local->ctxs + c.i,
		(local->ctxscnt - c.i) * sizeof *local->ctxs);
	local->ctxscnt += 1;
	local->ctxs[c.i] = ctx;
	ctx->local = local;
	return true;
}

static void ctx_unregister(struct ctx *ctx) {
	assert(ctx->local);
	pthread_mutex_lock(&ctx->local->mtx);
	struct _ctx c = _getctx(ctx->local, ctx->address);
	assert(c.ctx == ctx);
	if (ctx->local->ctxscnt > 1) {
		memmove(&ctx->local->ctxs[c.i], &ctx->local->ctxs[c.i + 1],
			(--ctx->local->ctxscnt - c.i) * sizeof *ctx->local->ctxs);
		ctx->local->ctxs = realloc(
			ctx->local->ctxs, ctx->local->ctxscnt * sizeof *ctx->local->ctxs);
		assert(ctx->local->ctxs); /* Size reduction should always work */
	} else {
		free(ctx->local->ctxs);
		ctx->local->ctxs = NULL;
		ctx->local->ctxscnt = 0;
	}
	pthread_mutex_unlock(&ctx->local->mtx);
	if (ctx->interface) {
		/* Free local as this is not server */
		local_destroy(ctx->local);
		free(ctx->local);
	}
	ctx->local = NULL;
}


static ssize_t cookie_read(void *cookie, char *buf, size_t size) {
	struct ctx *c = (struct ctx *)cookie;
	if (c->local == NULL)
		return 0;

	pthread_mutex_lock(&c->mtx);
	if (!RTOKEN_WAIT(true)) {
		c->rstate = RS_NONE;
		pthread_mutex_unlock(&c->mtx);
		return -1;
	}
	if (c->rstate != RS_MSG) {
		pthread_mutex_unlock(&c->mtx);
		return -1;
	}

	ssize_t siz = MIN(size, c->rsiz - c->roff);
	memcpy(buf, c->rdata + c->roff, siz);
	c->roff += siz;
	if (c->rsiz == c->roff && !(c->rcounter & 0x80))
		/* Do not pass if not read all or is the last frame. */
		RTOKEN_PASS;
	pthread_mutex_unlock(&c->mtx);
	return siz;
}

static ssize_t cookie_write(void *cookie, const char *buf, size_t size) {
	struct ctx *c = (struct ctx *)cookie;
	if (c->local == NULL)
		return 0; /* Not connected */

	size_t rsiz = size;
	while (rsiz) {
		if (canid(c->wframe) == 0) {
			canid(c->wframe) = c->local_address | CANID_SHVCAN | CANID_UNUSED |
				CANID_FIRST_MASK;
# ifdef SHVC_NUTTXCAN
			c->wframe.cm_hdr.ch_edl = 1;
			c->wframe.cm_hdr.ch_brs = 1;
# else
			c->wframe.flags = CANFD_BRS;
# endif
			candata(c->wframe)[0] = c->address;
			wframe_len(c) = 2;
		} else if (wframe_len(c) == 64) {
			if (!send_data_frame(c))
				return 0; /* On failure switch file stream to error */
			canid(c->wframe) = c->local_address | CANID_SHVCAN | CANID_UNUSED;
			candata(c->wframe)[0] = c->address;
			wframe_len(c) = 2;
		}
		size_t siz = MIN(rsiz, 64 - wframe_len(c));
		memcpy(&candata(c->wframe)[wframe_len(c)], buf, siz);
		wframe_len(c) += siz;
		buf += siz;
		rsiz -= siz;
	}
	return size;
}


static bool can_pack(void *ptr, const struct cpitem *item) {
	struct ctx *c = (struct ctx *)((char *)ptr - offsetof(struct ctx, pub.pack));
	rpclogger_log_item(c->pub.logger_out, item);
	if (canid(c->wframe) == 0)
		putc_unlocked(1, c->fw); /* Chainpack identifier */
	return chainpack_pack(c->fw, item) > 0;
}

static void can_unpack(void *ptr, struct cpitem *item) {
	struct ctx *c = (struct ctx *)((char *)ptr - offsetof(struct ctx, pub.unpack));
	chainpack_unpack(c->fr, item);
	rpclogger_log_item(c->pub.logger_in, item);
}

static int can_ctrl(rpcclient_t client, enum rpcclient_ctrlop op) {
	struct ctx *c = (struct ctx *)client;
	switch (op) {
		case RPCC_CTRLOP_DESTROY:
			if (c->local)
				ctx_unregister(c);
			close(c->reventfd);
			fclose(c->fr);
			fclose(c->fw);
			pthread_mutex_destroy(&c->mtx);
			pthread_cond_destroy(&c->rcond);
			pthread_cond_destroy(&c->wackcond);
			free(c);
			return true;
		case RPCC_CTRLOP_DISCONNECT:
			if (c->local)
				ctx_unregister(c);
			return true;
		case RPCC_CTRLOP_RESET:
			if (c->local == NULL) {
				if (c->interface) {
					struct local *local = malloc(sizeof *local);
					if (local == NULL)
						return false;
					if (!local_init(local, c->interface, c->local_address, -1)) {
						free(local);
						return false;
					}
					pthread_mutex_lock(&local->mtx);
					assert(ctx_register(local, c));
					pthread_mutex_unlock(&local->mtx);
					c->local = local;
				} else
					return false;
			}
			putc_unlocked(0, c->fw);
			rpclogger_log_reset(c->pub.logger_out);
			return can_ctrl(client, RPCC_CTRLOP_SENDMSG);
		case RPCC_CTRLOP_ERRNO: {
			int res = 0;
			pthread_mutex_lock(&c->mtx);
			res = c->local ? c->local->errnum : ENOTCONN;
			pthread_mutex_unlock(&c->mtx);
			return res;
		}
		case RPCC_CTRLOP_NEXTMSG:
			uint64_t _;
			assert(read(c->reventfd, &_, sizeof _) == sizeof _);
			pthread_mutex_lock(&c->mtx);
			assert(c->rtoken); /* otherwise why was reventfd set? */
			if (c->rstate == RS_DISCONNECT) {
				RTOKEN_PASS;
				pthread_mutex_unlock(&c->mtx);
				ctx_unregister(c);
				return RPCC_ERROR;
			}
			assert(c->rstate == RS_FIRST); /* The only other valid case. */
			/* Send ACK as this is the message we are recieving. */
			if (!send_ack(c, c->rcounter)) {
				pthread_mutex_unlock(&c->mtx);
				return RPCC_ERROR;
			}
			c->rstate = RS_MSG;
			assert(c->rsiz > 0 && c->roff == 0);
			c->roff = 1;
			switch (c->rdata[0]) {
				case 0: /* Reset requested */
					pthread_mutex_unlock(&c->mtx);
					rpclogger_log_reset(c->pub.logger_in);
					return can_ctrl(client, RPCC_CTRLOP_VALIDMSG) ? RPCC_RESET
																  : RPCC_NOTHING;
				case 1: /* Chainpack message */
					if (c->rsiz > 1) {
						pthread_mutex_unlock(&c->mtx);
						clearerr(c->fr);
						return RPCC_MESSAGE;
					} /* zero length messages are invalid anyway */
				default: /* Ignore other message types */
					c->rstate = RS_NONE;
					RTOKEN_PASS;
					pthread_mutex_unlock(&c->mtx);
					return RPCC_NOTHING;
			}
		case RPCC_CTRLOP_VALIDMSG: {
			pthread_mutex_lock(&c->mtx);
			while (true) {
				RTOKEN_PASS;
				if (c->rstate != RS_MSG || c->rcounter & 0x80)
					break;
				/* Receive rest of the message. */
				if (!RTOKEN_WAIT(true))
					c->rstate = RS_NONE;
			}
			bool res = c->rstate == RS_MSG;
			c->rstate = RS_NONE;
			pthread_mutex_unlock(&c->mtx);
			rpclogger_log_end(c->pub.logger_in,
				res ? RPCLOGGER_ET_VALID : RPCLOGGER_ET_INVALID);
			return res;
		}
		case RPCC_CTRLOP_IGNOREMSG:
			pthread_mutex_lock(&c->mtx);
			c->rstate = RS_NONE;
			RTOKEN_PASS;
			pthread_mutex_unlock(&c->mtx);
			rpclogger_log_end(c->pub.logger_in, RPCLOGGER_ET_UNKNOWN);
			return 0;
		case RPCC_CTRLOP_SENDMSG: {
			bool res = true;
			if (canid(c->wframe) != 0) {
				candata(c->wframe)[1] |= 0x80;
				memset(&candata(c->wframe)[wframe_len(c)], 0, 64 - wframe_len(c));
				if (!send_data_frame(c))
					res = false; // TODO record errno?
				canid(c->wframe) = 0;
				wframe_len(c) = 0;
				rpclogger_log_end(c->pub.logger_out,
					res ? RPCLOGGER_ET_VALID : RPCLOGGER_ET_INVALID);
			}
			return res;
		}
		case RPCC_CTRLOP_DROPMSG:
			if (canid(c->wframe) != 0) {
				if (!(canid(c->wframe) & CANID_FIRST_MASK)) {
					candata(c->wframe)[1] = (candata(c->wframe)[1] | 0x80) + 1;
					wframe_len(c) = 2;
					send_data_frame(c); // TODO record errno?
				}
				canid(c->wframe) = 0;
				wframe_len(c) = 0;
				rpclogger_log_end(c->pub.logger_out, RPCLOGGER_ET_INVALID);
			}
			return true;
		case RPCC_CTRLOP_CONTRACK:
			return false; /* Yes, CAN doesn't fully emulate conntrack. */
		case RPCC_CTRLOP_POLLFD:
			return c->reventfd;
	}
	/* This should not happen -> implementation error */
	abort(); // GCOVR_EXCL_LINE
}

static size_t can_peername(rpcclient_t client, char *buf, size_t siz) {
	struct ctx *c = (struct ctx *)client;
	const char *interface =
		c->interface ?: (c->local ? c->local->interface : "???");
	return snprintf(buf, siz, "can://%s:%d", interface, c->address);
}

static struct ctx *ctx_new(
	uint8_t address, uint8_t local_address, const char *reset_interface) {
	struct ctx *ctx = malloc(sizeof *ctx);
	if (ctx == NULL)
		return NULL;

	int fd = eventfd(0, 0);
	if (fd < 0)
		goto fdfail;
	FILE *fr = fopencookie(ctx, "r", (cookie_io_functions_t){.read = cookie_read});
	if (fr == NULL)
		goto frfail;
	setbuf(fr, NULL);
	FILE *fw =
		fopencookie(ctx, "w", (cookie_io_functions_t){.write = cookie_write});
	if (fw == NULL)
		goto fwfail;
	setbuf(fw, NULL);

	*ctx = (struct ctx){
		.pub =
			(const struct rpcclient){
				.ctrl = can_ctrl,
				.peername = can_peername,
				.pack = can_pack,
				.unpack = can_unpack,
			},
		.interface = reset_interface,
		.address = address,
		.local_address = local_address,
		.mtx = PTHREAD_MUTEX_INITIALIZER,

		.reventfd = fd,
		.rstate = RS_NONE,
		.rcond = PTHREAD_COND_INITIALIZER,

		.wack = true, /* Initial message is always accepted */
		.wackcond = PTHREAD_COND_INITIALIZER,

		.fr = fr,
		.fw = fw,
	};
	pthread_cond_init(&ctx->rcond, NULL);
	pthread_cond_init(&ctx->rrcond, NULL);
	pthread_cond_init(&ctx->wackcond, NULL);
	return ctx;

	fclose(fw);
fwfail:
	fclose(fr);
frfail:
	close(fd);
fdfail:
	free(ctx);
	return NULL;
}

static void ctx_free(struct ctx *ctx) {
	assert(ctx->local); /* Must be unregistered already */
	fclose(ctx->fr);
	fclose(ctx->fw);
	close(ctx->reventfd);
	free(ctx);
}

/* Client *********************************************************************/

rpcclient_t rpcclient_can_new(
	const char *interface, uint8_t address, uint8_t local_address) {
	if (local_address > 128) /* Not supported at the moment */
		return NULL;

	struct ctx *ctx = ctx_new(address, local_address, interface);
	if (ctx == NULL)
		return NULL;
	return &ctx->pub;
}

/* Server *********************************************************************/

struct server {
	struct rpcserver pub;
	int fd;
	struct local local;
};

static int can_server_ctrl(struct rpcserver *server, enum rpcserver_ctrlop op) {
	struct server *s = (struct server *)server;
	switch (op) {
		case RPCS_CTRLOP_POLLFD:
			return s->fd;
		case RPCS_CTRLOP_DESTROY:
			local_destroy(&s->local);
			free(s);
			return 0;
	}
	abort();
}

static rpcclient_t can_server_accept(struct rpcserver *server) {
	struct server *s = (struct server *)server;
	struct ctx *ctx;
	assert(read(s->fd, &ctx, sizeof ctx) == sizeof ctx);
	return &ctx->pub;
}

rpcserver_t rpcserver_can_new(const char *interface, uint8_t address) {
	if (address > 128) /* Not supported for now */
		return NULL;

	struct server *res = malloc(sizeof *res);
	if (res == NULL)
		return NULL;

	int fds[2];
	if (pipe(fds) == -1) {
		free(res);
		return NULL;
	}

	res->pub = (struct rpcserver){
		.ctrl = can_server_ctrl,
		.accept = can_server_accept,
	};
	res->fd = fds[0];
	if (!local_init(&res->local, interface, address, fds[1])) {
		free(res);
		return NULL;
	}

	return &res->pub;
}

#endif
