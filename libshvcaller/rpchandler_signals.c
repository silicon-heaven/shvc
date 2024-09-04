#include <stdlib.h>
#include <shv/rpchandler_signals.h>

#define MSGRETRY (5)

struct rpchandler_signals {
	rpchandler_signal_func_t func;
	void *cookie;
	struct subscription {
		const char *ri;
		time_t last_msg;
		int rid;
	} *subs;
	size_t cnt, siz;
	volatile _Atomic bool all_done;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};


static int cmpsubs(const void *a, const void *b) {
	struct subscription *const *da = a;
	struct subscription *const *db = b;
	return !strcmp((*da)->ri, (*db)->ri);
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_signals *handler_signals = cookie;
	switch (ctx->meta.type) {
		case RPCMSG_T_SIGNAL:
			pthread_mutex_lock(&handler_signals->lock);
			if (handler_signals->func) {
				handler_signals->func(handler_signals->cookie, ctx);
				pthread_mutex_unlock(&handler_signals->lock);
				return RPCHANDLER_MSG_DONE;
			}
			pthread_mutex_unlock(&handler_signals->lock);
			break;
		case RPCMSG_T_RESPONSE:
			if (!handler_signals->all_done) {
				pthread_mutex_lock(&handler_signals->lock);
				bool done = true;
				for (size_t i = 0; i < handler_signals->cnt; i++)
					if (ctx->meta.request_id == abs(handler_signals->subs[i].rid)) {
						if (handler_signals->subs[i].rid < 0) {
							free((char *)handler_signals->subs[i].ri);
							memcpy(&handler_signals->subs[i],
								&handler_signals->subs[i + 1],
								(--handler_signals->cnt - i) *
									sizeof *handler_signals->subs);
						} else
							handler_signals->subs[i].rid = 0;
						/* Now only check if we are not done with changes */
						for (i++; done && i < handler_signals->cnt; i++)
							if (handler_signals->subs[i].rid != 0)
								done = false;
						handler_signals->all_done = done;
						pthread_mutex_unlock(&handler_signals->lock);
						return RPCHANDLER_MSG_DONE;
					} else if (handler_signals->subs[i].rid != 0)
						done = false;
				pthread_mutex_unlock(&handler_signals->lock);
			}
			break;
		default:
			break;
	}
	return RPCHANDLER_MSG_SKIP;
}

static int rpc_idle(void *cookie, struct rpchandler_idle *ctx) {
	struct rpchandler_signals *handler_signals = cookie;
	int res = RPCHANDLER_IDLE_SKIP;
	if (!handler_signals->all_done) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		pthread_mutex_lock(&handler_signals->lock);
		for (size_t i = 0; i < handler_signals->cnt; i++) {
			if (handler_signals->subs[i].rid != 0) {
				time_t difft =
					handler_signals->subs[i].last_msg + MSGRETRY - ts.tv_sec;
				if (difft <= 0) {
					cp_pack_t pack = rpchandler_msg_new(ctx);
					if (pack) {
						rpcmsg_pack_request(pack, ".broker/currentClient",
							handler_signals->subs[i].rid > 0 ? "subscribe"
															 : "unsubscribe",
							NULL, abs(handler_signals->subs[i].rid));
						cp_pack_str(pack, handler_signals->subs[i].ri);
						cp_pack_container_end(pack);
						if (rpchandler_msg_send(ctx))
							handler_signals->subs[i].last_msg = ts.tv_sec;
					}
					pthread_mutex_unlock(&handler_signals->lock);
					return 0;
				} else if (res > (difft * 1000))
					res = difft * 1000;
			}
		}
		pthread_mutex_unlock(&handler_signals->lock);
	}
	return res;
}

static void rpc_reset(void *cookie) {
	struct rpchandler_signals *handler_signals = cookie;
	pthread_mutex_lock(&handler_signals->lock);
	/* Assign request ID to re-subscribe. */
	for (size_t i = 0; i < handler_signals->cnt; i++) {
		handler_signals->subs[i].last_msg = 0;
		handler_signals->subs[i].rid = rpcmsg_request_id();
	}
	handler_signals->all_done = false;
	pthread_mutex_unlock(&handler_signals->lock);
}

static const struct rpchandler_funcs rpc_funcs = {
	.msg = rpc_msg,
	.idle = rpc_idle,
	.reset = rpc_reset,
};

rpchandler_signals_t rpchandler_signals_new(
	rpchandler_signal_func_t func, void *cookie) {
	struct rpchandler_signals *res = malloc(sizeof *res);
	res->func = func;
	res->cookie = cookie;
	res->cnt = 0;
	res->siz = 4;
	res->subs = malloc(res->siz * sizeof *res->subs);
	res->all_done = true;
	pthread_mutex_init(&res->lock, NULL);
	pthread_cond_init(&res->cond, NULL);
	return res;
}

void rpchandler_signals_destroy(rpchandler_signals_t rpchandler_signals) {
	if (rpchandler_signals == NULL)
		return;
	for (size_t i = 0; i < rpchandler_signals->cnt; i++)
		free((char *)rpchandler_signals->subs[i].ri);
	free(rpchandler_signals->subs);
	free(rpchandler_signals);
}

struct rpchandler_stage rpchandler_signals_stage(
	rpchandler_signals_t rpchandler_signals) {
	return (struct rpchandler_stage){
		.funcs = &rpc_funcs, .cookie = rpchandler_signals};
}

void rpchandler_signals_subscribe(
	rpchandler_signals_t handler_signals, const char *ri) {
	pthread_mutex_lock(&handler_signals->lock);
	struct subscription ref = {.ri = ri};
	struct subscription *sub = bsearch(
		&ref, handler_signals->subs, handler_signals->cnt, sizeof ref, cmpsubs);
	if (!sub) {
		if ((handler_signals->cnt + 1) >= handler_signals->siz)
			handler_signals->subs = realloc(handler_signals->subs,
				(handler_signals->siz *= 2) * sizeof *handler_signals->subs);
		handler_signals->subs[handler_signals->cnt++] =
			(struct subscription){.ri = strdup(ri), .rid = rpcmsg_request_id()};
		qsort(handler_signals->subs, handler_signals->cnt,
			sizeof *handler_signals->subs, cmpsubs);
		handler_signals->all_done = false;
	}
	pthread_mutex_unlock(&handler_signals->lock);
}

void rpchandler_signals_unsubscribe(
	rpchandler_signals_t handler_signals, const char *ri) {
	pthread_mutex_lock(&handler_signals->lock);
	struct subscription ref = {.ri = ri};
	struct subscription *sub = bsearch(
		&ref, handler_signals->subs, handler_signals->cnt, sizeof ref, cmpsubs);
	if (sub) {
		sub->last_msg = 0;
		sub->rid = -rpcmsg_request_id();
		handler_signals->all_done = false;
	}
	pthread_mutex_unlock(&handler_signals->lock);
}

bool rpchandler_signals_status(rpchandler_signals_t rpchandler_signals) {
	return rpchandler_signals->all_done;
}

bool rpchandler_signals_wait(
	rpchandler_signals_t rpchandler_signals, struct timespec *abstime) {
	bool res = true;
	pthread_mutex_lock(&rpchandler_signals->lock);
	if (!rpchandler_signals->all_done) {
		if (abstime)
			res = pthread_cond_timedwait(&rpchandler_signals->cond,
					  &rpchandler_signals->lock, abstime) == 0;
		else
			pthread_cond_wait(&rpchandler_signals->cond, &rpchandler_signals->lock);
	}
	pthread_mutex_unlock(&rpchandler_signals->lock);
	return res;
}
