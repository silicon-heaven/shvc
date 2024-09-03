#include <stdlib.h>
#include <shv/rpchandler_responses.h>


struct rpchandler_responses {
	struct rpcresponse {
		int request_id;
		rpcresponse_callback_t callback;
		void *cookie;
		pthread_cond_t cond;
		struct rpchandler_responses *_Atomic owner;
		struct rpcresponse *next;
	} *resp;
	pthread_mutex_t lock;
};

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_responses *resp = cookie;
	if (ctx->meta.type != RPCMSG_T_RESPONSE && ctx->meta.type != RPCMSG_T_ERROR)
		return RPCHANDLER_MSG_SKIP;
	pthread_mutex_lock(&resp->lock);
	rpcresponse_t *pr = &resp->resp;
	while (*pr) {
		rpcresponse_t r = *pr;
		if (r->request_id == ctx->meta.request_id) {
			if (r->callback(ctx, r->cookie)) {
				*pr = r->next;
				r->owner = NULL;
				pthread_cond_signal(&r->cond);
			}
			pthread_mutex_unlock(&resp->lock);
			return RPCHANDLER_MSG_DONE;
		}
		pr = &r->next;
	};
	pthread_mutex_unlock(&resp->lock);
	return RPCHANDLER_MSG_SKIP;
}

static struct rpchandler_funcs rpc_funcs = {.msg = rpc_msg};

rpchandler_responses_t rpchandler_responses_new(void) {
	struct rpchandler_responses *res = malloc(sizeof *res);
	res->resp = NULL;
	pthread_mutex_init(&res->lock, NULL);
	return res;
}

void rpchandler_responses_destroy(rpchandler_responses_t resp) {
	rpcresponse_t r = resp->resp;
	while (r) {
		rpcresponse_t pr = r;
		r = r->next;
		free(pr);
	};
	pthread_mutex_destroy(&resp->lock);
	free(resp);
}

struct rpchandler_stage rpchandler_responses_stage(
	rpchandler_responses_t responses) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = responses};
}


rpcresponse_t rpcresponse_expect(rpchandler_responses_t responses,
	int request_id, rpcresponse_callback_t func, void *cookie) {
	rpcresponse_t res = malloc(sizeof *res);
	res->request_id = request_id;
	res->callback = func;
	res->cookie = cookie;
	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	pthread_cond_init(&res->cond, &attr);
	pthread_condattr_destroy(&attr);
	res->owner = responses;
	res->next = NULL;

	pthread_mutex_lock(&responses->lock);
	rpcresponse_t *r = &responses->resp;
	while (*r)
		r = &(*r)->next;
	*r = res;
	pthread_mutex_unlock(&responses->lock);

	return res;
}

int rpcresponse_request_id(rpcresponse_t response) {
	return response->request_id;
}

void rpcresponse_discard(rpcresponse_t response) {
	if (response == NULL)
		return;

	struct rpchandler_responses *responses = response->owner;
	if (responses) {
		pthread_mutex_lock(&responses->lock);
		rpcresponse_t *pr = &responses->resp;
		while (*pr) {
			if (*pr == response) {
				*pr = response->next;
				break;
			}
			pr = &(*pr)->next;
		}
		pthread_mutex_unlock(&responses->lock);
	}
	pthread_cond_destroy(&response->cond);
	free(response);
}

bool rpcresponse_waitfor(rpcresponse_t response, int timeout) {
	struct rpchandler_responses *responses = response->owner;
	if (responses != NULL) {
		struct timespec ts_timeout;
		clock_gettime(CLOCK_MONOTONIC, &ts_timeout);
		ts_timeout.tv_sec += timeout / 1000;
		ts_timeout.tv_nsec += (timeout % 1000) * 1000000;
		pthread_mutex_lock(&responses->lock);
		/* Note that sporadic wakeup can't because we are the only one waiting */
		int cr = pthread_cond_timedwait(
			&response->cond, &responses->lock, &ts_timeout);
		pthread_mutex_unlock(&responses->lock);
		if (cr != 0) /* Covers timeout as well as interrupt */
			return false;
	}
	pthread_cond_destroy(&response->cond);
	free(response);
	return true;
}


rpcresponse_t rpcresponse_send_request_void(rpchandler_t handler,
	rpchandler_responses_t responses, const char *path, const char *method,
	const char *uid, rpcresponse_callback_t func, void *ctx) {
	cp_pack_t pack = rpchandler_msg_new(handler);
	if (pack == NULL)
		return NULL;
	int request_id = rpcmsg_request_id();
	if (!rpcmsg_pack_request_void(pack, path, method, uid, request_id)) {
		rpchandler_msg_drop(handler);
		return NULL;
	}
	rpcresponse_t res = rpcresponse_expect(responses, request_id, func, ctx);
	if (!rpchandler_msg_send(handler)) {
		rpcresponse_discard(res);
		return NULL;
	}
	return res;
}
