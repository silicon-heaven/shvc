#include <shv/rpchandler.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

struct rpchandler {
	const rpchandler_func **funcs;
	const struct rpcmsg_meta_limits *meta_limits;

	struct rpcreceive {
		rpcclient_t client;
		/* Prevent from sending multiple messages.
		 * Sending in the receive thread can very easily result in deadlock if
		 * two SHVC handlers are put against each other. On the other hand it is
		 * very convenient to immediately respond to the request and thus we do
		 * a compromise, we allow sending one message after request and nothing
		 * more.
		 */
		bool can_respond;
		/* We use two mutexes to implement priority locking. The thread
		 * receiving messages needs to have priority over others and thus it has
		 * a dedicated lock.  Regular locks need to check if they can lock the
		 * prioprity lock and just release the regular lock if they can't. The
		 * priority thread takes priority lock to force others to yield the
		 * primary lock.
		 */
		pthread_mutex_t mutex, pmutex;
	} recv;

	int nextrid;
	struct obstack obs;
};

struct rpchandler_response {
	rpcclient_t client;
	pthread_mutex_t *mutex, *pmutex;
};

rpchandler_t rpchandler_new(rpcclient_t client, const rpchandler_func **funcs,
	const struct rpcmsg_meta_limits *limits) {
	struct rpchandler *res = malloc(sizeof *res);
	res->funcs = funcs;
	res->meta_limits = limits;

	res->recv.client = client;
	pthread_mutex_init(&res->recv.mutex, NULL);
	pthread_mutex_init(&res->recv.pmutex, NULL);

	/* There can be login in front of this which uses 1-3 as request ID.
	 * Technically we could use here even 1, because those requests are already
	 * handled and thus no collision can happen, but let's not.
	 */
	res->nextrid = 4;
	obstack_init(&res->obs);
	return res;
}

void rpchandler_destroy(rpchandler_t rpchandler) {
	if (rpchandler == NULL)
		return;
	obstack_free(&rpchandler->obs, NULL);
	pthread_mutex_destroy(&rpchandler->recv.mutex);
	pthread_mutex_destroy(&rpchandler->recv.pmutex);
	free(rpchandler);
}

bool rpchandler_next(struct rpchandler *rpchandler) {
	if (!(rpcclient_nextmsg(rpchandler->recv.client)))
		return false;

	bool res = false;
	void *obs_base = obstack_base(&rpchandler->obs);
	struct cpitem item;
	cpitem_unpack_init(&item);
	struct rpcmsg_meta meta;
	if (!rpcmsg_head_unpack(rpcclient_unpack(rpchandler->recv.client), &item,
			&meta, NULL, &rpchandler->obs))
		goto done;

	rpchandler->recv.can_respond = meta.type == RPCMSG_T_REQUEST;
	enum rpchandler_func_res fres = RPCHFR_UNHANDLED;
	for (const rpchandler_func **h = rpchandler->funcs;
		 *h != NULL && fres != RPCHFR_UNHANDLED; h++) {
		fres = (***h)((void *)*h, &rpchandler->recv,
			rpcclient_unpack(rpchandler->recv.client), &item, &meta);
	}

	if (fres == RPCHFR_UNHANDLED) {
		/* Default handler for requests, other types are dropped. */
		if (!rpcclient_validmsg(rpchandler->recv.client))
			goto done;
		if (meta.type == RPCMSG_T_REQUEST) {
			cp_pack_t pack = rpcreceive_response_new(&rpchandler->recv);
			rpcmsg_pack_ferror(pack, &meta, RPCMSG_E_METHOD_NOT_FOUND,
				"No such method '%s' on path '%s'", meta.method, meta.path);
			rpcreceive_response_send(&rpchandler->recv);
		}
	}
	// TODO handle other fres

	res = true;
done:
	obstack_free(&rpchandler->obs, obs_base);
	return res;
}

static void *thread_loop(void *ctx) {
	rpchandler_t rpchandler = ctx;
	while (rpchandler_next(rpchandler)) {}
	return NULL;
}

int rpchandler_spawn_thread(rpchandler_t rpchandler, pthread_t *restrict thread,
	const pthread_attr_t *restrict attr) {
	return pthread_create(thread, attr, thread_loop, rpchandler);
}

int rpchandler_next_request_id(rpchandler_t rpchandler) {
	return rpchandler->nextrid++;
}


cp_pack_t rpchandler_msg_new(rpchandler_t rpchandler) {
	while (true) {
		pthread_mutex_lock(&rpchandler->recv.mutex);
		if (pthread_mutex_trylock(&rpchandler->recv.pmutex) == 0) {
			pthread_mutex_unlock(&rpchandler->recv.pmutex);
			break;
		} else
			pthread_mutex_unlock(&rpchandler->recv.mutex);
	}
	return rpcclient_pack(rpchandler->recv.client);
}

bool rpchandler_msg_send(rpchandler_t rpchandler) {
	bool res = rpcclient_sendmsg(rpchandler->recv.client);
	pthread_mutex_unlock(&rpchandler->recv.mutex);
	return res;
}

bool rpchandler_msg_drop(rpchandler_t rpchandler) {
	bool res = rpcclient_dropmsg(rpchandler->recv.client);
	pthread_mutex_unlock(&rpchandler->recv.mutex);
	return res;
}


bool rpcreceive_validmsg(rpcreceive_t receive) {
	return rpcclient_validmsg(receive->client);
}

cp_pack_t rpcreceive_response_new(rpcreceive_t receive) {
	assert(receive->can_respond);
	pthread_mutex_lock(&receive->pmutex);
	pthread_mutex_lock(&receive->mutex);
	pthread_mutex_unlock(&receive->pmutex);
	return rpcclient_pack(receive->client);
}

bool rpcreceive_response_send(rpcreceive_t receive) {
	assert(receive->can_respond);
	bool res = rpcclient_sendmsg(receive->client);
	receive->can_respond = false;
	pthread_mutex_unlock(&receive->mutex);
	return res;
}

bool rpcreceive_response_drop(rpcreceive_t receive) {
	assert(receive->can_respond);
	bool res = rpcclient_dropmsg(receive->client);
	pthread_mutex_unlock(&receive->mutex);
	return res;
}
