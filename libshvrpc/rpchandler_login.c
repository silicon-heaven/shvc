#include <stdlib.h>
#include <shv/rpchandler_impl.h>
#include <shv/rpchandler_login.h>
#include <shv/sha1.h>

#define HELLO_RID 1
#define LOGIN_RID 2
#define HELLO_TIMEOUT (1)
#define LOGIN_TIMEOUT (5)


struct rpchandler_login {
	const struct rpclogin *opts;
	volatile _Atomic bool logged;
	volatile _Atomic rpcerrno_t errnum;
	char *errmsg;
	pthread_cond_t cond;
	pthread_mutex_t condm;
	struct timespec last_request;
	char nonce[SHV_NONCE_MAXLEN + 1];
};

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_login *handler_login = cookie;
	if (handler_login->logged)
		return RPCHANDLER_MSG_SKIP;

	if (ctx->meta.type == RPCMSG_T_RESPONSE) {
		switch (ctx->meta.request_id) {
			case HELLO_RID:
				// TODO has param check
				if (cp_unpack_type(ctx->unpack, ctx->item) == CPITEM_MAP) {
					for_cp_unpack_map(ctx->unpack, ctx->item, key, 5) {
						if (!strcmp("nonce", key)) {
							cp_unpack_strncpy(ctx->unpack, ctx->item,
								handler_login->nonce, 32);
							break; /* Nothing more is needed from this map */
						} else
							cp_unpack_skip(ctx->unpack, ctx->item);
					}
				}
				if (!rpchandler_msg_valid(ctx))
					handler_login->nonce[0] = '\0';
				else
					handler_login->last_request = (struct timespec){};
				return RPCHANDLER_MSG_DONE;
			case LOGIN_RID:
				if (rpchandler_msg_valid(ctx)) {
					pthread_mutex_lock(&handler_login->condm);
					handler_login->logged = true;
					pthread_cond_broadcast(&handler_login->cond);
					pthread_mutex_unlock(&handler_login->condm);
				}
				return RPCHANDLER_MSG_DONE;
		}
	} else if (ctx->meta.type == RPCMSG_T_ERROR &&
		(ctx->meta.request_id == HELLO_RID || ctx->meta.request_id == LOGIN_RID)) {
		rpcerrno_t errnum;
		char *errmsg;
		rpcerror_unpack(ctx->unpack, ctx->item, &errnum, &errmsg);
		if (rpchandler_msg_valid(ctx)) {
			pthread_mutex_lock(&handler_login->condm);
			handler_login->errmsg = errmsg;
			handler_login->errnum = errnum;
			pthread_cond_broadcast(&handler_login->cond);
			pthread_mutex_unlock(&handler_login->condm);
			return RPCHANDLER_MSG_STOP;
		} else
			free(handler_login->errmsg);
		return RPCHANDLER_MSG_DONE;
	}

	rpchandler_msg_valid(ctx); /* Just ignore anything else */
	return RPCHANDLER_MSG_DONE;
}

static int rpc_idle(void *cookie, struct rpchandler_idle *ctx) {
	struct rpchandler_login *handler_login = cookie;
	if (handler_login->logged) {
		struct timespec t;
		assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
		/* Note: The seconds precission is enough for idle timeout */
		// TODO make idle time configurable
		int res = (RPC_DEFAULT_IDLE_TIME / 2) - t.tv_sec + ctx->last_send.tv_sec;
		if (res <= 0) {
			/* Ping */
			cp_pack_t pack = rpchandler_msg_new(ctx);
			if (pack == NULL)
				return 0;
			rpcmsg_pack_request_void(pack, ".app", "ping", NULL, 3);
			rpchandler_msg_send(ctx);
			return (RPC_DEFAULT_IDLE_TIME / 2) * 1000;
		} else
			return res * 1000;
	}

	if (handler_login->errnum != RPCERR_NO_ERROR)
		return RPCHANDLER_IDLE_STOP;

	bool has_nonce = handler_login->nonce[0] != '\0';

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int diff = (has_nonce ? LOGIN_TIMEOUT : HELLO_TIMEOUT) -
		(ts.tv_sec - handler_login->last_request.tv_sec);
	if (diff > 0)
		return RPCHANDLER_IDLE_LAST(diff * 1000);

	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return RPCHANDLER_IDLE_LAST(0);

	handler_login->last_request = ts;
	if (!has_nonce) {
		/* Hello */
		rpcmsg_pack_request_void(pack, "", "hello", NULL, HELLO_RID);
		rpchandler_msg_send(ctx);
		return HELLO_TIMEOUT * 1000;
	} else {
		/* Login */
		rpcmsg_pack_request(pack, "", "login", NULL, LOGIN_RID);
		rpclogin_pack(pack, handler_login->opts, handler_login->nonce, false);
		cp_pack_container_end(pack);
		rpchandler_msg_send(ctx);
		return LOGIN_TIMEOUT * 1000;
	}
}

static void rpc_reset(void *cookie) {
	struct rpchandler_login *handler_login = cookie;
	handler_login->logged = false;
	handler_login->nonce[0] = '\0';
	handler_login->errnum = RPCERR_NO_ERROR;
	handler_login->errmsg = NULL;
	handler_login->last_request = (struct timespec){};
}

static const struct rpchandler_funcs rpc_funcs = {
	.msg = rpc_msg,
	.idle = rpc_idle,
	.reset = rpc_reset,
};

rpchandler_login_t rpchandler_login_new(const struct rpclogin *login) {
	struct rpchandler_login *res = malloc(sizeof *res);
	res->opts = login;
	res->logged = false;
	res->nonce[0] = '\0';
	res->nonce[32] = '\0'; /* We do not use this byte except as end of string */
	res->errnum = RPCERR_NO_ERROR;
	res->errmsg = NULL;
	res->last_request = (struct timespec){};
	pthread_mutex_init(&res->condm, NULL);
	pthread_cond_init(&res->cond, NULL);
	return res;
}

void rpchandler_login_destroy(rpchandler_login_t handler_login) {
	pthread_cond_destroy(&handler_login->cond);
	pthread_mutex_destroy(&handler_login->condm);
	free(handler_login->errmsg);
	free(handler_login);
}

struct rpchandler_stage rpchandler_login_stage(rpchandler_login_t handler_login) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = handler_login};
}

bool rpchandler_login_status(rpchandler_login_t rpchandler_login,
	rpcerrno_t *errnum, const char **errmsg) {
	if (errnum || errmsg) {
		pthread_mutex_lock(&rpchandler_login->condm);
		if (errnum)
			*errnum = rpchandler_login->errnum;
		if (errmsg)
			*errmsg = rpchandler_login->errmsg;
		pthread_mutex_unlock(&rpchandler_login->condm);
	}
	return rpchandler_login->logged && rpchandler_login->errnum == RPCERR_NO_ERROR;
}

bool rpchandler_login_wait(rpchandler_login_t rpchandler_login,
	rpcerrno_t *errnum, const char **errmsg, struct timespec *abstime) {
	bool res = true;
	pthread_mutex_lock(&rpchandler_login->condm);

	if (!rpchandler_login->logged && rpchandler_login->errnum == RPCERR_NO_ERROR) {
		if (abstime)
			res = pthread_cond_timedwait(&rpchandler_login->cond,
					  &rpchandler_login->condm, abstime) == 0;
		else
			pthread_cond_wait(&rpchandler_login->cond, &rpchandler_login->condm);
	}

	if (errnum)
		*errnum = rpchandler_login->errnum;
	if (errmsg)
		*errmsg = rpchandler_login->errmsg;
	res = res && rpchandler_login->errnum == RPCERR_NO_ERROR;
	pthread_mutex_unlock(&rpchandler_login->condm);
	return res;
}
