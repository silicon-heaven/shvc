#include <shv/rpchandler_responses.h>
#include <stdlib.h>
#include <semaphore.h>

struct rpchandler_responses {
	struct rpcresponse {
		int request_id;
		struct rpcreceive *receive;
		cp_unpack_t unpack;
		struct cpitem *item;
		const struct rpcmsg_meta *meta;
		sem_t sem, sem_complete;
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
	rpcresponse_t pr = NULL;
	rpcresponse_t r = resp->resp;
	while (r) {
		if (r->request_id == meta->request_id) {
			r->receive = receive;
			r->meta = meta;
			sem_post(&r->sem);
			sem_wait(&r->sem_complete);
			sem_destroy(&r->sem);
			sem_destroy(&r->sem_complete);
			if (pr)
				pr->next = r->next;
			else
				resp->resp = r->next;
			free(r);
			break;
		}
		pr = r;
		r = r->next;
	};
	pthread_mutex_unlock(&resp->lock);
	return true;
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


rpcresponse_t rpcresponse_expect(rpchandler_responses_t responses, int request_id) {
	rpcresponse_t res = malloc(sizeof *res);
	res->request_id = request_id;
	res->receive = NULL;
	sem_init(&res->sem, false, 0);
	sem_init(&res->sem_complete, false, 0);
	res->next = NULL;

	pthread_mutex_lock(&responses->lock);
	rpcresponse_t *r = &responses->resp;
	while (*r)
		r = &(*r)->next;
	*r = res;
	pthread_mutex_unlock(&responses->lock);

	return res;
}

void rpcresponse_discard(rpchandler_responses_t responses, rpcresponse_t response) {
	if (response == NULL)
		return;
	pthread_mutex_lock(&responses->lock);

	/* The response might have already been received and if so we must only
	 * release the read lock here. We do this inside mutex lock to prevent race
	 * condition where rpc_msg would submit it after we checked it.
	 */
	if (sem_trywait(&response->sem) == 0) {
		sem_post(&response->sem_complete);
	} else {
		rpcresponse_t r = responses->resp;
		while (r && r->next != response)
			r = r->next;
		if (r)
			r->next = r->next->next;
		sem_destroy(&response->sem);
		sem_destroy(&response->sem_complete);
		free(response);
	}

	pthread_mutex_unlock(&responses->lock);
}

rpcresponse_t rpcresponse_send_request_void(rpchandler_t handler,
	rpchandler_responses_t responses, const char *path, const char *method) {
	cp_pack_t packer = rpchandler_msg_new(handler);
	int request_id = rpchandler_next_request_id(handler);
	if (!rpcmsg_pack_request_void(packer, path, method, request_id)) {
		rpchandler_msg_drop(handler); /* Release the lock */
		return NULL;
	}
	rpcresponse_t res = rpcresponse_expect(responses, request_id);
	if (!rpchandler_msg_send(handler)) {
		rpcresponse_discard(responses, res);
		return NULL;
	}
	return res;
}

int rpcresponse_request_id(rpcresponse_t response) {
	return response->request_id;
}

bool rpcresponse_waitfor(rpcresponse_t response, struct rpcreceive **receive,
	const struct rpcmsg_meta **meta, int timeout) {
	struct timespec ts_timeout;
	clock_gettime(CLOCK_REALTIME, &ts_timeout);
	ts_timeout.tv_sec += timeout;

	int sem_ret = sem_timedwait(&response->sem, &ts_timeout);
	if (sem_ret != 0)
		return false;

	*meta = response->meta;
	*receive = response->receive;
	return true;
}

bool rpcresponse_validmsg(rpcresponse_t response) {
	bool res = rpcreceive_validmsg(response->receive);
	sem_post(&response->sem_complete);
	return res;
}
