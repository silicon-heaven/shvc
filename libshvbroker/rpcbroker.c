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
	res->use_lock = !(flags & RPCBROKER_F_NOLOCK);
	if (res->use_lock)
		pthread_mutex_init(&res->lock, NULL);
	res->name = name;
	res->login = login;
	res->login_cookie = login_cookie;
	res->clients_cnt = 4;
	res->clients = malloc(res->clients_cnt * sizeof *res->clients);
	memset(res->clients, 0, res->clients_cnt * sizeof *res->clients);
	ARR_INIT(res->mounts);
	ARR_INIT(res->subscriptions);
	return res;
}

void rpcbroker_destroy(rpcbroker_t broker) {
	if (broker == NULL)
		return;
	if (broker->use_lock)
		pthread_mutex_destroy(&broker->lock);
	/* Note that all clients should be already unregistered */
	// TODO possibly do no rely on that
	free(broker->clients);
	free(broker);
}


int rpcbroker_client_register(rpcbroker_t broker, rpchandler_t handler,
	struct rpchandler_stage *access_stage, struct rpchandler_stage *stage,
	const struct rpcbroker_role *role) {
	broker_lock(broker);
	int cid = cid_new(broker);
	struct clientctx *ctx = broker->clients + cid;
	ctx->lastuse = IN_USE;
	ctx->handler = handler;
	ctx->activity_timeout = role ? -1 : IDLE_TIMEOUT_LOGIN;
	ctx->last_activity = get_time();
	ctx->role = role;
	ctx->broker = broker;
	if (role && role_assign(ctx, role) != ROLE_RES_OK) {
		ctx->lastuse = 0;
		return -1;
	}
	*access_stage =
		(struct rpchandler_stage){.funcs = &access_funcs, .cookie = ctx};
	*stage = (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = ctx};
	broker_unlock(broker);
	return cid;
}

void rpcbroker_client_unregister(rpcbroker_t broker, int client_id) {
	broker_lock(broker);
	struct clientctx *ctx = &broker->clients[client_id];
	role_unassign(ctx);
	ctx->nonce[0] = '\0';
	free(ctx->username);
	ctx->username = NULL;
	unsubscribe_all(broker, client_id);
	ARR_RESET(ctx->ttlsubs);
	// TODO ensure that nobody is using it through broker from other threads
	cid_free(broker, client_id);
	broker_unlock(broker);
}

rpchandler_t rpcbroker_client_handler(rpcbroker_t broker, int client_id) {
	if (!cid_valid(broker, client_id))
		return NULL;
	return broker->clients[client_id].handler;
}
