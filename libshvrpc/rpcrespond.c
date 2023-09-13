#include <shv/rpcrespond.h>
#include <stdlib.h>

struct rpcresponder {
	rpchandler_func func;
	struct rpcrespond {
		int request_id;
		rpcreceive_t receive;
		cp_unpack_t unpack;
		struct cpitem *item;
		const struct rpcmsg_meta *meta;
		sem_t sem, sem_complete;
		struct rpcrespond *next;
	} * resp;
	pthread_mutex_t lock;
};

static enum rpchandler_func_res func(void *ptr, rpcreceive_t receive,
	cp_unpack_t unpack, struct cpitem *item, const struct rpcmsg_meta *meta) {
	struct rpcresponder *resp = ptr;
	if (meta->type != RPCMSG_T_RESPONSE && meta->type != RPCMSG_T_ERROR)
		return RPCHFR_UNHANDLED;
	enum rpchandler_func_res res = RPCHFR_HANDLED;
	pthread_mutex_lock(&resp->lock);
	rpcrespond_t pr = NULL;
	rpcrespond_t r = resp->resp;
	while (r) {
		if (r->request_id == meta->request_id) {
			r->receive = receive;
			r->unpack = unpack;
			r->item = item;
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

rpcresponder_t rpcresponder_new(void) {
	struct rpcresponder *res = malloc(sizeof *res);
	res->func = func;
	res->resp = NULL;
	pthread_mutex_init(&res->lock, NULL);
	return res;
}

void rpcresponder_destroy(rpcresponder_t resp) {
	rpcrespond_t r = resp->resp;
	while (r) {
		rpcrespond_t pr = r;
		r = r->next;
		free(pr);
	};
	pthread_mutex_destroy(&resp->lock);
	free(resp);
}

rpchandler_func *rpcresponder_func(rpcresponder_t responder) {
	return &responder->func;
}


rpcrespond_t rpcrespond_expect(rpcresponder_t responder, int request_id) {
	rpcrespond_t res = malloc(sizeof *res);
	res->request_id = request_id;
	res->receive = NULL;
	sem_init(&res->sem, false, 0);
	sem_init(&res->sem_complete, false, 0);

	pthread_mutex_lock(&responder->lock);
	rpcrespond_t *r = &responder->resp;
	while (*r)
		r = &(*r)->next;
	*r = res;
	pthread_mutex_unlock(&responder->lock);

	return res;
}

bool rpcrespond_waitfor(rpcrespond_t respond, cp_unpack_t *unpack,
	struct cpitem **item, int timeout) {
	sem_wait(&respond->sem);
	return 1;
}

bool rpcrespond_validmsg(rpcrespond_t respond) {
	bool res = rpcreceive_validmsg(respond->receive);
	sem_post(&respond->sem_complete);
	return res;
}
