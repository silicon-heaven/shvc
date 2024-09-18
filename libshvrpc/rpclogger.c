#include <stdlib.h>
#include <sys/socket.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <syslog.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>

static const size_t boundary = 6; /* RESET\0 and ...\n\0 */

struct rpclogger {
	rpclogger_func_t callback;
	FILE *f;
	char *buf;
	size_t buflen, bufsiz, prefixlen;
	bool ellipsis;
	struct cpon_state cpon_state;
	unsigned maxdepth;
};


static ssize_t logwrite(void *cookie, const char *buf, size_t size) {
	struct rpclogger *logger = cookie;
	size_t space = logger->bufsiz - logger->buflen - boundary;
	if (space < size && size <= 3) {
		rpclogger_log_flush(logger);
		space = logger->bufsiz - logger->buflen - boundary;
	}
	if (space < size)
		size = space;
	memcpy(logger->buf + logger->buflen, buf, size);
	logger->buflen += size;
	if (logger->buflen >= logger->bufsiz)
		rpclogger_log_flush(logger);
	return size;
}

static void cpon_state_realloc(struct cpon_state *state) {
	const struct rpclogger *logger =
		(void *)state - offsetof(struct rpclogger, cpon_state);
	size_t cnt = state->cnt ? state->cnt * 2 : 1;
	if (cnt > logger->maxdepth)
		cnt = logger->maxdepth;
	if (cnt <= state->cnt)
		return;
	struct cpon_state_ctx *ctx = realloc(state->ctx, cnt * sizeof *state->ctx);
	if (ctx) {
		state->cnt = cnt;
		state->ctx = ctx;
	}
}

rpclogger_t rpclogger_new(rpclogger_func_t callback, const char *prefix,
	size_t bufsiz, unsigned maxdepth) {
	if (maxdepth == 0)
		return NULL;
	size_t prefixlen = strlen(prefix);
	/* We have here additional space for boundary, plus one token */
	if (prefixlen + boundary + 4 >= bufsiz)
		return NULL; /* Prefix can't fit to buffer so just drop */
	struct rpclogger *res = malloc(sizeof *res);
	*res = (struct rpclogger){
		.callback = callback,
		.f = fopencookie(res, "w", (cookie_io_functions_t){.write = logwrite}),
		.buf = malloc(bufsiz),
		.buflen = prefixlen,
		.bufsiz = bufsiz,
		.prefixlen = prefixlen,
		.ellipsis = false,
		.cpon_state = (struct cpon_state){.realloc = cpon_state_realloc},
		.maxdepth = maxdepth,
	};
	setbuf(res->f, NULL);
	strncpy(res->buf, prefix, prefixlen + 1);
	return res;
}

void rpclogger_destroy(rpclogger_t logger) {
	if (logger == NULL)
		return;
	free(logger->buf);
	free(logger->cpon_state.ctx);
	free(logger);
}

static void ellipsis(rpclogger_t logger) {
	if (logger->ellipsis)
		return;
	for (int i = 0; i < 3; i++)
		logger->buf[logger->buflen++] = '.';
	logger->ellipsis = true;
}

void rpclogger_log_item(rpclogger_t logger, const struct cpitem *item) {
	if (logger == NULL)
		return;
	if (logger->buflen == logger->prefixlen && logger->cpon_state.depth > 0)
		ellipsis(logger);
	if ((item->type == CPITEM_BLOB || item->type == CPITEM_STRING) &&
		item->buf == NULL) {
		/* Blob and string skipping. Data is not received. */
		struct cpitem nitem = *item;
		nitem.as.Blob.flags &= ~CPBI_F_HEX;
		nitem.rchr = "...";
		nitem.as.Blob.len = (logger->ellipsis) ? 3 : 0;
		cpon_pack(logger->f, &logger->cpon_state, &nitem);
		logger->ellipsis = true;
	} else {
		cpon_pack(logger->f, &logger->cpon_state, item);
		logger->ellipsis = false;
	}
}

void rpclogger_log_reset(rpclogger_t logger) {
	if (logger == NULL)
		return;
	fputs("RESET", logger->f);
}

void rpclogger_log_end(rpclogger_t logger, enum rpclogger_end_type tp) {
	if (logger == NULL)
		return;
	if (logger->cpon_state.depth == 0 && tp == RPCLOGGER_ET_UNKNOWN)
		return;
	if (tp != RPCLOGGER_ET_VALID) {
		ellipsis(logger);
		if (tp == RPCLOGGER_ET_INVALID)
			logger->buf[logger->buflen - 1] = '!';
		else if (tp == RPCLOGGER_ET_UNKNOWN)
			logger->buf[logger->buflen - 1] = '?';
	}
	rpclogger_log_flush(logger);
	logger->cpon_state.depth = 0;
	logger->ellipsis = false;
}

void rpclogger_log_flush(rpclogger_t logger) {
	if (logger == NULL)
		return;
	if (logger->buflen == logger->prefixlen)
		return;
	if (!logger->ellipsis && logger->cpon_state.depth > 0)
		ellipsis(logger);
	logger->buf[logger->buflen++] = '\n';
	logger->buf[logger->buflen] = '\0';
	logger->callback(logger->buf);
	logger->buflen = logger->prefixlen;
}

void rpclogger_func_stderr(const char *line) {
	fputs(line, stderr);
}

void rpclogger_func_syslog_debug(const char *line) {
	syslog(LOG_DEBUG, "%s", line);
}
