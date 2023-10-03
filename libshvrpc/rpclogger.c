#include <shv/rpcclient.h>
#include <stdlib.h>
#include <assert.h>
#include <semaphore.h>
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
	sem_t semaphore;
};


void rpclogger_log_lock(struct rpclogger *logger, bool in) {
	if (logger == NULL)
		return;
	// TODO signal interrupt
	sem_wait(&logger->semaphore);
	fputs(in ? "<= " : "=> ", logger->f);
}

void rpclogger_log_item(struct rpclogger *logger, const struct cpitem *item) {
	if (logger == NULL)
		return;
	int semval;
	assert(sem_getvalue(&logger->semaphore, &semval) == 0);
	if (semval > 0)
		return; /* Semaphore is not taken so do not log */

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
	sem_post(&logger->semaphore);
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
	sem_init(&res->semaphore, false, 1);
	return res;
}

void rpclogger_destroy(rpclogger_t logger) {
	free(logger->cpon_state.ctx);
	sem_destroy(&logger->semaphore);
	free(logger);
}
