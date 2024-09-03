#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <obstack.h>
#include <semaphore.h>
#include <time.h>
#include <shv/rpchandler.h>
#include <shv/rpchandler_impl.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#include "strset.h"

struct rpchandler {
	const struct rpchandler_stage *stages;
	const struct rpcmsg_meta_limits *meta_limits;
	rpcclient_t client;
	/* Lock for handler specific variables here */
	pthread_mutex_t lock;

	/*! Last time we sent message.
	 *
	 * This is provided to stages. The primary use case is in broker connection
	 * where clients must keep communication up by sending messages here and
	 * there.
	 */
	struct timespec last_send;

	struct obstack obstack;
	/* The thread receiving messages needs to have priority over others. Other
	 * threads need to release the lock immediately right after taking it if
	 * `send_priority` is `true`.
	 */
	pthread_mutex_t send_lock;
	volatile _Atomic bool send_priority;
};

struct msg_ctx {
	struct rpchandler_msg ctx;
	rpchandler_t handler;
	bool msg_valid;
};

struct ls_ctx {
	struct rpchandler_ls ctx;
	struct msg_ctx *mctx;
	cp_pack_t pack;
	struct strset strset;
	bool located;
};

struct dir_ctx {
	struct rpchandler_dir ctx;
	struct msg_ctx *mctx;
	cp_pack_t pack;
	bool located;
};

struct idle_ctx {
	struct rpchandler_idle ctx;
	rpchandler_t handler;
	bool msg_sent;
};


rpchandler_t rpchandler_new(rpcclient_t client,
	const struct rpchandler_stage *stages,
	const struct rpcmsg_meta_limits *limits) {
	struct rpchandler *res = malloc(sizeof *res);
	res->stages = stages;
	res->meta_limits = limits;
	res->client = client;
	pthread_mutex_init(&res->lock, NULL);
	clock_gettime(CLOCK_MONOTONIC, &res->last_send);
	obstack_init(&res->obstack);
	pthread_mutex_init(&res->send_lock, NULL);
	res->send_priority = false;
	return res;
}

void rpchandler_destroy(rpchandler_t handler) {
	if (handler == NULL)
		return;
	obstack_free(&handler->obstack, NULL);
	pthread_mutex_destroy(&handler->lock);
	pthread_mutex_destroy(&handler->send_lock);
	free(handler);
}

const struct rpchandler_stage *rpchandler_stages(rpchandler_t handler) {
	return handler->stages;
}

void rpchandler_change_stages(
	rpchandler_t handler, const struct rpchandler_stage *stages) {
	pthread_mutex_lock(&handler->lock);
	handler->stages = stages;
	pthread_mutex_unlock(&handler->lock);
}

rpcclient_t rpchandler_client(rpchandler_t handler) {
	return handler->client;
}

static void priority_send_lock(rpchandler_t handler) {
	handler->send_priority = true;
	pthread_mutex_lock(&handler->send_lock);
	handler->send_priority = false;
}

static void send_lock(rpchandler_t handler) {
	while (true) {
		pthread_mutex_lock(&handler->send_lock);
		if (!handler->send_priority)
			break;
		pthread_mutex_unlock(&handler->send_lock);
		sched_yield(); /* Just so we would not try to lock it immediately again
						*/
	}
}

static void send_unlock(rpchandler_t handler) {
	pthread_mutex_unlock(&handler->send_lock);
}


static bool common_ls_dir(struct msg_ctx *ctx, char **name) {
	*name = NULL;
	bool invalid_param = false;
	if (rpcmsg_has_value(ctx->ctx.item)) {
		switch (cp_unpack_type(ctx->ctx.unpack, ctx->ctx.item)) {
			case CPITEM_NULL:
			case CPITEM_LIST: /* TODO backward compatible */
				break;
			case CPITEM_STRING:
				*name = cp_unpack_strdupo(
					ctx->ctx.unpack, ctx->ctx.item, &ctx->handler->obstack);
				break;
			default:
				invalid_param = true;
				break;
		}
	}
	if (!rpchandler_msg_valid(&ctx->ctx))
		return false;

	if (!invalid_param)
		return true;
	rpchandler_msg_send_error(
		&ctx->ctx, RPCERR_INVALID_PARAM, "Use Null or String with node name");
	return false;
}

static bool valid_path(rpchandler_t handler, char *path) {
	if (path == NULL || *path == '\0')
		return true; /* root node is always valid */
	char *slash = strrchr(path, '/');
	if (slash)
		*slash = '\0';
	struct ls_ctx lsctx = (struct ls_ctx){
		.ctx.path = slash ? path : "",
		.ctx.name = slash ? slash + 1 : path,
		.strset = (struct strset){},
		.located = false,
	};
	for (const struct rpchandler_stage *s = handler->stages;
		 s->funcs && !lsctx.located; s++)
		if (s->funcs->ls)
			s->funcs->ls(s->cookie, &lsctx.ctx);
	if (slash)
		*slash = '/';
	return lsctx.located;
}

static bool handle_ls(struct msg_ctx *ctx) {
	char *name;
	if (!common_ls_dir(ctx, &name))
		return true;

	struct ls_ctx lsctx = (struct ls_ctx){
		.ctx.path = ctx->ctx.meta.path ?: "",
		.ctx.name = name,
		.mctx = ctx,
		.pack = NULL,
		.strset = (struct strset){},
		.located = false,
	};
	for (const struct rpchandler_stage *s = ctx->handler->stages;
		 s->funcs && !lsctx.located; s++)
		if (s->funcs->ls)
			s->funcs->ls(s->cookie, &lsctx.ctx);

	if (!lsctx.located && lsctx.strset.cnt == 0 &&
		!valid_path(ctx->handler, ctx->ctx.meta.path)) {
		rpchandler_msg_send_error(
			&ctx->ctx, RPCERR_METHOD_NOT_FOUND, "No such node");
		return true;
	}
	if (lsctx.ctx.name == NULL) {
		if (lsctx.pack == NULL) {
			lsctx.pack = rpchandler_msg_new_response(&ctx->ctx);
			cp_pack_list_begin(lsctx.pack);
		}
		cp_pack_container_end(lsctx.pack);
		shv_strset_free(&lsctx.strset);
	} else {
		lsctx.pack = rpchandler_msg_new_response(&ctx->ctx);
		cp_pack_bool(lsctx.pack, lsctx.located);
	}
	cp_pack_container_end(lsctx.pack);
	rpchandler_msg_send(&ctx->ctx);
	return true;
}

static bool handle_dir(struct msg_ctx *ctx) {
	char *name;
	if (!common_ls_dir(ctx, &name))
		return true;
	if (!valid_path(ctx->handler, ctx->ctx.meta.path)) {
		rpchandler_msg_send_error(
			&ctx->ctx, RPCERR_METHOD_NOT_FOUND, "No such node");
		return true;
	}

	cp_pack_t pack = rpchandler_msg_new_response(&ctx->ctx);
	struct dir_ctx dirctx = (struct dir_ctx){
		.ctx.name = name,
		.ctx.path = ctx->ctx.meta.path,
		.mctx = ctx,
		.pack = pack,
		.located = false,
	};
	if (dirctx.ctx.name == NULL)
		cp_pack_list_begin(dirctx.pack);
	rpchandler_dir_result(&dirctx.ctx, &rpcdir_dir);
	rpchandler_dir_result(&dirctx.ctx, &rpcdir_ls);
	for (const struct rpchandler_stage *s = ctx->handler->stages;
		 s->funcs && !dirctx.located; s++)
		if (s->funcs->dir)
			s->funcs->dir(s->cookie, &dirctx.ctx);
	if (dirctx.ctx.name == NULL)
		cp_pack_container_end(dirctx.pack);
	else
		cp_pack_bool(dirctx.pack, dirctx.located);
	cp_pack_container_end(dirctx.pack);
	rpchandler_msg_send(&ctx->ctx);
	return true;
}

static bool handle_msg(struct msg_ctx *ctx) {
	for (const struct rpchandler_stage *s = ctx->handler->stages; s->funcs; s++)
		if (s->funcs->msg) {
			enum rpchandler_msg_res res = s->funcs->msg(s->cookie, &ctx->ctx);
			if (res != RPCHANDLER_MSG_SKIP)
				return res == RPCHANDLER_MSG_DONE;
		}

	if (ctx->ctx.meta.type == RPCMSG_T_REQUEST &&
		!strcmp(ctx->ctx.meta.method, "ls"))
		return handle_ls(ctx);
	else if (ctx->ctx.meta.type == RPCMSG_T_REQUEST &&
		!strcmp(ctx->ctx.meta.method, "dir"))
		return handle_dir(ctx);

	if (rpchandler_msg_valid(&ctx->ctx) && ctx->ctx.meta.type == RPCMSG_T_REQUEST)
		rpchandler_msg_send_method_not_found(&ctx->ctx);
	return true;
}

bool rpchandler_next(struct rpchandler *handler) {
	bool res = true;
	pthread_mutex_lock(&handler->lock);
	switch (rpcclient_nextmsg(handler->client)) {
		case RPCC_MESSAGE:
			void *obs_base = obstack_base(&handler->obstack);
			struct cpitem item;
			cpitem_unpack_init(&item);
			struct msg_ctx ctx;
			ctx.ctx.item = &item;
			ctx.handler = handler;
			ctx.ctx.unpack = rpcclient_unpack(handler->client);
			if (rpcmsg_head_unpack(ctx.ctx.unpack, &item, &ctx.ctx.meta, NULL,
					&handler->obstack)) {
				res = handle_msg(&ctx);
			}
			obstack_free(&handler->obstack, obs_base);
			break;
		case RPCC_RESET:
			for (const struct rpchandler_stage *s = handler->stages; s->funcs; s++)
				if (s->funcs->reset)
					s->funcs->reset(s->cookie);
			break;
		case RPCC_ERROR: /* Handled by rpcclient_connected */
		case RPCC_NOTHING:
			break; /* Nothing to do */
	}
	pthread_mutex_unlock(&handler->lock);
	return res && rpcclient_connected(handler->client);
}

int rpchandler_idling(rpchandler_t handler) {
	pthread_mutex_lock(&handler->lock);
	struct idle_ctx ctx = {
		.ctx.last_send = handler->last_send,
		.handler = handler,
		.msg_sent = false,
	};
	int res = RPCHANDLER_IDLE_SKIP;
	for (const struct rpchandler_stage *s = handler->stages;
		 s->funcs && res > 0; s++) {
		if (s->funcs->idle) {
			int t = s->funcs->idle(s->cookie, &ctx.ctx);
			if (res > t)
				res = t;
		}
	}
	pthread_mutex_unlock(&handler->lock);
	return res == RPCHANDLER_IDLE_STOP ? -1 : abs(res);
}

void rpchandler_run(rpchandler_t handler, volatile sig_atomic_t *halt) {
	int timeout = 0;
	struct pollfd pfd = {
		.fd = rpcclient_pollfd(handler->client),
		.events = POLLIN | POLLHUP,
	};
	while (timeout >= 0 && (!halt || !*halt)) {
		int pr = poll(&pfd, 1, timeout);
		int cancelstate;
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelstate);
		if (pr != 0) {
			timeout = 0;
			if ((pr == -1 && errno != EINTR) || pfd.revents & POLLERR)
				abort(); // TODO
			/* Even on POLLHUP we request so RPC Client actually detest
			 * the disconnect not just us.
			 */
			if (pfd.revents & (POLLIN | POLLHUP) && !rpchandler_next(handler))
				timeout = -1;
		} else
			timeout = rpchandler_idling(handler);
		pthread_setcancelstate(cancelstate, NULL);
	}
}

static void *thread_loop(void *_ctx) {
	rpchandler_t handler = _ctx;
	rpchandler_run(handler, NULL);
	return NULL;
}

int rpchandler_spawn_thread(rpchandler_t handler, pthread_t *restrict thread,
	const pthread_attr_t *restrict attr) {
	// TODO set higher priority to this thread than parent has. This is to
	// hopefully win all locks without having other threads to release it when
	// this one waits for it.
	return pthread_create(thread, attr, thread_loop, handler);
}


cp_pack_t _rpchandler_msg_new(rpchandler_t handler) {
	send_lock(handler);
	return rpcclient_pack(handler->client);
}
cp_pack_t _rpchandler_impl_msg_new(struct rpchandler_msg *ctx) {
	struct msg_ctx *mctx = (struct msg_ctx *)ctx;
	if (ctx->unpack != NULL || ctx->meta.type != RPCMSG_T_REQUEST)
		return NULL;
	priority_send_lock(mctx->handler);
	return rpcclient_pack(mctx->handler->client);
}
cp_pack_t _rpchandler_idle_msg_new(struct rpchandler_idle *ctx) {
	struct idle_ctx *ictx = (struct idle_ctx *)ctx;
	if (ictx->msg_sent)
		return NULL;
	priority_send_lock(ictx->handler);
	return rpcclient_pack(ictx->handler->client);
}

bool _rpchandler_msg_send(rpchandler_t handler) {
	bool res = rpcclient_sendmsg(handler->client);
	clock_gettime(CLOCK_MONOTONIC, &handler->last_send);
	send_unlock(handler);
	return res;
}
bool _rpchandler_impl_msg_send(struct rpchandler_msg *ctx) {
	struct msg_ctx *mctx = (struct msg_ctx *)ctx;
	if (ctx->meta.type != RPCMSG_T_REQUEST)
		return false;
	return _rpchandler_msg_send(mctx->handler);
}
bool _rpchandler_idle_msg_send(struct rpchandler_idle *ctx) {
	struct idle_ctx *ictx = (struct idle_ctx *)ctx;
	if (ictx->msg_sent)
		return false;
	if (!_rpchandler_msg_send(ictx->handler))
		return false;
	ictx->msg_sent = true;
	return true;
}

bool _rpchandler_msg_drop(rpchandler_t handler) {
	bool res = rpcclient_dropmsg(handler->client);
	send_unlock(handler);
	return res;
}
bool _rpchandler_impl_msg_drop(struct rpchandler_msg *ctx) {
	struct msg_ctx *mctx = (struct msg_ctx *)ctx;
	if (ctx->meta.type != RPCMSG_T_REQUEST)
		return false;
	return _rpchandler_msg_drop(mctx->handler);
}
bool _rpchandler_idle_msg_drop(struct rpchandler_idle *ctx) {
	struct idle_ctx *ictx = (struct idle_ctx *)ctx;
	if (ictx->msg_sent)
		return false;
	return _rpchandler_msg_drop(ictx->handler);
}


bool rpchandler_msg_valid(struct rpchandler_msg *ctx) {
	struct msg_ctx *mctx = (struct msg_ctx *)ctx;
	if (ctx->unpack != NULL) {
		mctx->msg_valid = rpcclient_validmsg(mctx->handler->client);
		ctx->unpack = NULL;
	}
	return mctx->msg_valid;
}


static void pack_ls_result(struct ls_ctx *lsctx, const char *name) {
	if (lsctx->pack == NULL) {
		lsctx->pack = rpchandler_msg_new_response(&lsctx->mctx->ctx);
		cp_pack_list_begin(lsctx->pack);
	}
	cp_pack_str(lsctx->pack, name);
}
void rpchandler_ls_result(struct rpchandler_ls *ctx, const char *name) {
	struct ls_ctx *lsctx = (struct ls_ctx *)ctx;
	if (ctx->name)
		lsctx->located = lsctx->located || !strcmp(ctx->name, name);
	else if (shv_strset_add(&lsctx->strset, name))
		pack_ls_result(lsctx, name);
}
void rpchandler_ls_result_const(struct rpchandler_ls *ctx, const char *name) {
	struct ls_ctx *lsctx = (struct ls_ctx *)ctx;
	if (ctx->name)
		lsctx->located = lsctx->located || !strcmp(ctx->name, name);
	else if (shv_strset_add_const(&lsctx->strset, name))
		pack_ls_result(lsctx, name);
}
void rpchandler_ls_result_fmt(struct rpchandler_ls *ctx, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	rpchandler_ls_result_vfmt(ctx, fmt, args);
	va_end(args);
}
void rpchandler_ls_result_vfmt(
	struct rpchandler_ls *ctx, const char *fmt, va_list args) {
	struct ls_ctx *lsctx = (struct ls_ctx *)ctx;
	if (ctx->name && lsctx->located)
		return; /* Faster return when already located */
	char *str;
	assert(vasprintf(&str, fmt, args) > 0);
	if (ctx->name) {
		lsctx->located = !strcmp(ctx->name, str);
	} else if (shv_strset_add_dyn(&lsctx->strset, str)) {
		pack_ls_result(lsctx, str);
		return;
	}
	free(str);
}

void rpchandler_ls_exists(struct rpchandler_ls *ctx) {
	struct ls_ctx *lsctx = (struct ls_ctx *)ctx;
	if (ctx->name)
		lsctx->located = true;
}

void rpchandler_dir_result(struct rpchandler_dir *ctx, const struct rpcdir *method) {
	struct dir_ctx *dirctx = (struct dir_ctx *)ctx;
	if (ctx->name)
		dirctx->located = dirctx->located || !strcmp(ctx->name, method->name);
	else
		rpcdir_pack(dirctx->pack, method);
}

void rpchandler_dir_exists(struct rpchandler_dir *ctx) {
	struct dir_ctx *dirctx = (struct dir_ctx *)ctx;
	if (ctx->name)
		dirctx->located = true;
}

bool rpchandler_msg_send_method_not_found(struct rpchandler_msg *ctx) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	rpcmsg_pack_ferror(pack, &ctx->meta, RPCERR_METHOD_NOT_FOUND,
		"No such method '%s' on path '%s'", ctx->meta.method, ctx->meta.path);
	return rpchandler_msg_send(ctx);
}


struct obstack *_rpchandler_msg_obstack(struct rpchandler_msg *ctx) {
	struct msg_ctx *mctx = (struct msg_ctx *)ctx;
	return &mctx->handler->obstack;
}
struct obstack *_rpchandler_idle_obstack(struct rpchandler_idle *ctx) {
	struct idle_ctx *ictx = (struct idle_ctx *)ctx;
	return &ictx->handler->obstack;
}
