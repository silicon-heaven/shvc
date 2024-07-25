#include <shv/rpchandler_login.h>
#include <shv/rpchandler_impl.h>
#include <shv/sha1.h>
#include <stdlib.h>
#include <limits.h>

#define HELLO_TIMEOUT (5000)
#define LOGIN_TIMEOUT (10000)


struct rpchandler_login {
	const struct rpclogin *opts;
	volatile _Atomic bool logged;
	volatile _Atomic rpcerrno_t errnum;
	char *errmsg;
	pthread_cond_t cond;
	pthread_mutex_t condm;
	char nonce[SHV_NONCE_MAXLEN + 1];
};

static bool rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_login *handler_login = cookie;
	if (handler_login->logged)
		return false;

	if (ctx->meta.type == RPCMSG_T_RESPONSE) {
		switch (ctx->meta.request_id) {
			case 1: /* hello */
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
				return true;
			case 2: /* login */
				if (rpchandler_msg_valid(ctx)) {
					pthread_mutex_lock(&handler_login->condm);
					handler_login->logged = true;
					pthread_cond_broadcast(&handler_login->cond);
					pthread_mutex_unlock(&handler_login->condm);
				}
				return true;
		}
	} else if (ctx->meta.type == RPCMSG_T_ERROR) {
		rpcerrno_t errnum;
		char *errmsg;
		rpcerror_unpack(ctx->unpack, ctx->item, &errnum, &errmsg);
		if (rpchandler_msg_valid(ctx)) {
			pthread_mutex_lock(&handler_login->condm);
			handler_login->errmsg = errmsg;
			handler_login->errnum = errnum;
			pthread_cond_broadcast(&handler_login->cond);
			pthread_mutex_unlock(&handler_login->condm);
		} else
			free(handler_login->errmsg);
		return true;
	}

	rpchandler_msg_valid(ctx); /* Just ignore anything else */
	return true;
}

int rpc_idle(void *cookie, struct rpchandler_idle *ctx) {
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
		return INT_MAX; /* Just bail out */

	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return 0;

	if (handler_login->nonce[0] == '\0') {
		/* Hello */
		rpcmsg_pack_request_void(pack, "", "hello", NULL, 1);
		rpchandler_msg_send(ctx);
		return HELLO_TIMEOUT;
	}

	/* Login */
	rpcmsg_pack_request(pack, "", "login", NULL, 2);
	rpclogin_pack(pack, handler_login->opts, handler_login->nonce, false);
	cp_pack_container_end(pack);
	rpchandler_msg_send(ctx);
	return LOGIN_TIMEOUT;
}

static const struct rpchandler_funcs rpc_funcs = {
	.msg = rpc_msg,
	.idle = rpc_idle,
};

rpchandler_login_t rpchandler_login_new(const struct rpclogin *login) {
	struct rpchandler_login *res = malloc(sizeof *res);
	res->opts = login;
	res->logged = false;
	res->nonce[0] = '\0';
	res->nonce[32] = '\0'; /* We do not use this byte except as end of string */
	res->errnum = RPCERR_NO_ERROR;
	res->errmsg = NULL;
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

bool rpchandler_login_status(rpchandler_login_t rpchandler_login) {
	return rpchandler_login->logged;
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

	int tmp = rpchandler_login->errnum;
	if (errnum)
		*errnum = tmp;
	if (errmsg)
		*errmsg = rpchandler_login->errmsg;
	res = res && tmp == RPCERR_NO_ERROR;
	pthread_mutex_unlock(&rpchandler_login->condm);
	return res;
}
