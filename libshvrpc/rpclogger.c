#include <shv/rpcclient.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <shv/rpcmsg.h>

struct rpclogger {
	FILE *f;
	bool ellipsis;
	struct cpon_state cpon_state;
	unsigned maxdepth;
	pthread_mutex_t lock;
};


void rpclogger_log_lock(struct rpclogger *logger, bool in) {
	if (logger == NULL)
		return;
	// TODO signal interrupt
	// TODO try lock instead of just lock and identify if we are the one
	pthread_mutex_lock(&logger->lock);
	fputs(in ? "<= " : "=> ", logger->f);
}

void rpclogger_log_item(struct rpclogger *logger, const struct cpitem *item) {
	if (logger == NULL)
		return;

	if (item->type == CPITEM_RAW) {
		/* We do not want to print raw data and thus just replace it with dots */
		if (!logger->ellipsis)
			fputs("...", logger->f);
		logger->ellipsis = true;
	} else if ((item->type == CPITEM_BLOB || item->type == CPITEM_STRING) &&
		item->buf == NULL) {
		struct cpitem nitem = *item;
		nitem.as.Blob.flags &= ~CPBI_F_HEX;
		nitem.rchr = "...";
		nitem.as.Blob.len = (nitem.as.Blob.flags & CPBI_F_FIRST) ? 3 : 0;
		cpon_pack(logger->f, &logger->cpon_state, &nitem);
		logger->ellipsis = false;
	} else {
		cpon_pack(logger->f, &logger->cpon_state, item);
		logger->ellipsis = false;
	}
}

void rpclogger_log_unlock(struct rpclogger *logger) {
	if (logger == NULL)
		return;
	if (logger->cpon_state.depth > 0) {
		/* if we skip rest of the message */
		logger->cpon_state.depth = 0;
		if (!logger->ellipsis)
			fputs("...", logger->f);
		logger->ellipsis = false;
	}
	fputc('\n', logger->f);
	pthread_mutex_unlock(&logger->lock);
}


static void cpon_state_realloc(struct cpon_state *state) {
	const struct rpclogger *logger =
		(void *)state - offsetof(struct rpclogger, cpon_state);
	size_t newcnt = state->cnt ? state->cnt * 2 : 1;
	if (newcnt > logger->maxdepth)
		newcnt = logger->maxdepth;
	if (newcnt > state->cnt) {
		state->cnt = newcnt;
		state->ctx = realloc(state->ctx, state->cnt * sizeof *state->ctx);
	}
}

rpclogger_t rpclogger_new(FILE *f, unsigned maxdepth) {
	struct rpclogger *res = malloc(sizeof *res);
	res->f = f;
	res->ellipsis = false;
	res->cpon_state = (struct cpon_state){.realloc = cpon_state_realloc};
	res->maxdepth = maxdepth;
	pthread_mutex_init(&res->lock, NULL);
	return res;
}

void rpclogger_destroy(rpclogger_t logger) {
	if (logger == NULL)
		return;

	free(logger->cpon_state.ctx);
	pthread_mutex_destroy(&logger->lock);
	free(logger);
}
