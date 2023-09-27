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
	} * resp;
	pthread_mutex_t lock;
};

static enum rpchandler_func_res rpc_msg(
	void *cookie, struct rpcreceive *receive, const struct rpcmsg_meta *meta) {
	struct rpchandler_responses *resp = cookie;
	if (meta->type != RPCMSG_T_RESPONSE && meta->type != RPCMSG_T_ERROR)
		return RPCHFR_UNHANDLED;
	enum rpchandler_func_res res = RPCHFR_HANDLED;
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
	return res;
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
	rpchandler_responses_t responder) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = responder};
}


rpcresponse_t rpcresponse_expect(rpchandler_responses_t responder, int request_id) {
	rpcresponse_t res = malloc(sizeof *res);
	res->request_id = request_id;
	res->receive = NULL;
	sem_init(&res->sem, false, 0);
	sem_init(&res->sem_complete, false, 0);

	pthread_mutex_lock(&responder->lock);
	rpcresponse_t *r = &responder->resp;
	while (*r)
		r = &(*r)->next;
	*r = res;
	pthread_mutex_unlock(&responder->lock);

	return res;
}

bool rpcresponse_waitfor(rpcresponse_t respond, struct rpcreceive **receive,
	struct rpcmsg_meta **meta, int timeout) {
	sem_wait(&respond->sem);
	// TODO wait
	return 1;
}

bool rpcresponse_validmsg(rpcresponse_t respond) {
	bool res = rpcreceive_validmsg(respond->receive);
	sem_post(&respond->sem_complete);
	return res;
}
