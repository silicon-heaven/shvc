#include <limits.h>
#include <shv/rpcbroker.h>
#include <shv/rpchandler_impl.h>

#include "broker.h"
#include "stages.h"

// TODO we must change default meta limits to record extra arguments and also to
// propagate them when packing mesasge.
// TODO we should limit the mount point length to not overextend the
// default rpcmsg_meta_limits.


rpcbroker_t rpcbroker_new(
	const char *name, rpcbroker_login_t login, void *login_cookie, int flags) {
	struct rpcbroker *res = malloc(sizeof *res);
	if (!(flags & RPCBROKER_F_NOLOCK))
		pthread_mutex_init(&res->lock, NULL);
	res->name = name;
	res->flags = flags;
	res->login = login;
	res->login_cookie = login_cookie;
	res->clients_siz = 4;
	res->clients = calloc(res->clients_siz, sizeof *res->clients);
	res->clients_lastuse = calloc(res->clients_siz, sizeof *res->clients_lastuse);
	ARR_INIT(res->mounts);
	ARR_INIT(res->subscriptions);
	return res;
}

void rpcbroker_destroy(rpcbroker_t broker) {
	if (broker == NULL)
		return;
	if (!(broker->flags & RPCBROKER_F_NOLOCK))
		pthread_mutex_destroy(&broker->lock);
	/* Note that all clients should be already unregistered */
	// TODO possibly do no rely on that
	free(broker->clients);
	free(broker->clients_lastuse);
	free(broker);
}


int rpcbroker_client_register(rpcbroker_t broker, rpchandler_t handler,
	struct rpchandler_stage *access_stage, struct rpchandler_stage *stage,
	const struct rpcbroker_role *role) {
	broker_lock(broker);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	int cid;
	for (cid = 0; cid < broker->clients_siz &&
		 (broker->clients[cid] ||
			 broker->clients_lastuse[cid] >= now.tv_sec - REUSE_TIMEOUT);
		 cid++) {}
	if (cid == broker->clients_siz) {
		broker->clients_siz *= 2;
		struct clientctx **nclients =
			realloc(broker->clients, broker->clients_siz * sizeof *nclients);
		assert(nclients); // TODO
		time_t *nlastuse = realloc(
			broker->clients_lastuse, broker->clients_siz * sizeof *nlastuse);
		assert(nlastuse); // TODO
		memset(nclients + cid, 0, (broker->clients_siz / 2) * sizeof *nclients);
		memset(nlastuse + cid, 0, (broker->clients_siz / 2) * sizeof *nlastuse);
		broker->clients = nclients;
		broker->clients_lastuse = nlastuse;
	}

	struct clientctx *ctx = malloc(sizeof *ctx);
	ctx->cid = cid;
	broker->clients[cid] = ctx;
	ctx->broker = broker;
	ctx->handler = handler;
	ctx->role = role;
	ctx->nonce[0] = '\0';
	ctx->username = NULL;
	ctx->activity_timeout = role ? -1 : IDLE_TIMEOUT_LOGIN;
	ctx->last_activity = now.tv_sec;
	ARR_INIT(ctx->ttlsubs);
	if (role && role_assign(ctx, role) != ROLE_RES_OK) {
		broker->clients_lastuse[cid] = 0;
		free(ctx);
		return -1;
	}
	*access_stage =
		(struct rpchandler_stage){.funcs = &access_funcs, .cookie = ctx};
	*stage = (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = ctx};

	broker_unlock(broker);
	return cid;
}

void rpcbroker_client_unregister(rpcbroker_t broker, int client_id) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	broker_lock(broker);
	struct clientctx *ctx = broker->clients[client_id];
	broker->clients[client_id] = NULL;
	broker->clients_lastuse[client_id] = now.tv_sec;
	role_unassign(ctx);
	unsubscribe_all(broker, client_id);
	free(ctx->username);
	free(ctx->ttlsubs);
	free(ctx);
	broker_unlock(broker);
	if (!(broker->flags & RPCBROKER_F_NOLOCK)) {
		// TODO ensure that nobody is using it through broker from other threads
	}
}

rpchandler_t rpcbroker_client_handler(rpcbroker_t broker, int client_id) {
	if (cid_valid(broker, client_id))
		return broker->clients[client_id]->handler;
	return NULL;
}
