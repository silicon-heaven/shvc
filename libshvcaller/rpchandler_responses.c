#include <shv/rpchandler_responses.h>
#include <stdlib.h>
#include <semaphore.h>

struct rpchandler_responses {
	struct rpcresponse {
		int request_id;
		rpcresponse_callback_t callback;
		void *ctx;
		sem_t sem;
		struct rpchandler_responses *owner;
		struct rpcresponse *next;
	} *resp;
	pthread_mutex_t lock;
};

static bool rpc_msg(
	void *cookie, struct rpcreceive *receive, const struct rpcmsg_meta *meta) {
	struct rpchandler_responses *resp = cookie;
	if (meta->type != RPCMSG_T_RESPONSE && meta->type != RPCMSG_T_ERROR)
		return false;
	pthread_mutex_lock(&resp->lock);
	rpcresponse_t *pr = &resp->resp;
	while (*pr) {
		rpcresponse_t r = *pr;
		if (r->request_id == meta->request_id) {
			if (r->callback(receive, meta, r->ctx)) {
				*pr = r->next;
				sem_post(&r->sem);
			}
			pthread_mutex_unlock(&resp->lock);
			return true;
		}
		pr = &r->next;
	};
	pthread_mutex_unlock(&resp->lock);
	return false;
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
	int request_id, rpcresponse_callback_t func, void *ctx) {
	rpcresponse_t res = malloc(sizeof *res);
	res->request_id = request_id;
	res->callback = func;
	res->ctx = ctx;
	sem_init(&res->sem, false, 0);
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

	pthread_mutex_t *lock = &response->owner->lock;
	pthread_mutex_lock(lock);
	if (response->owner->resp != response) {
		rpcresponse_t r = response->owner->resp;
		while (r && r->next != response)
			r = r->next;
		if (r)
			r->next = r->next->next;
	} else
		response->owner->resp = NULL;
	sem_destroy(&response->sem);
	free(response);
	pthread_mutex_unlock(lock);
}

bool rpcresponse_waitfor(rpcresponse_t response, int timeout) {
	struct timespec ts_timeout;
	clock_gettime(CLOCK_REALTIME, &ts_timeout);
	ts_timeout.tv_sec += timeout;
	bool res = sem_timedwait(&response->sem, &ts_timeout) == 0;
	if (res) {
		sem_destroy(&response->sem);
		free(response);
	}
	return res;
}


rpcresponse_t rpcresponse_send_request_void(rpchandler_t handler,
	rpchandler_responses_t responses, const char *path, const char *method,
	rpcresponse_callback_t func, void *ctx) {
	int request_id = rpchandler_next_request_id(handler);
	if (!rpchandler_msg_new_request_void(handler, path, method, request_id))
		return NULL;
	rpcresponse_t res = rpcresponse_expect(responses, request_id, func, ctx);
	if (!rpchandler_msg_send(handler)) {
		rpcresponse_discard(res);
		return NULL;
	}
	return res;
}
