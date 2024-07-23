#include <shv/rpchandler.h>
#include <shv/rpchandler_impl.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include <semaphore.h>
#include <limits.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include "strset.h"

struct rpchandler {
	const struct rpchandler_stage *stages;
	const struct rpcmsg_meta_limits *meta_limits;
	rpcclient_t client;
	/* Lock for handler specific variables here */
	pthread_mutex_t lock;

	/*! The last time we received message.
	 *
	 * This can be used by RPC Broker to detect inactive clients.
	 */
	struct timespec last_receive;
	/*! Last time we sent message.
	 *
	 * This can be used by RPC clients to detect that thay should perform some
	 * activity to stay connected to the RPC Broker.
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
	pthread_mutex_init(&res->lock, NULL);

	res->client = client;
	clock_gettime(CLOCK_MONOTONIC, &res->last_receive);
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
	pthread_mutex_destroy(&handler->send_lock);
	free(handler);
}

void rpchandler_change_stages(
	rpchandler_t handler, const struct rpchandler_stage *stages) {
	pthread_mutex_lock(&handler->lock);
	handler->stages = stages;
	pthread_mutex_unlock(&handler->lock);
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
		&ctx->ctx, RPCERR_INVALID_PARAMS, "Use Null or String with node name");
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

static void handle_ls(struct msg_ctx *ctx) {
	char *name;
	if (!common_ls_dir(ctx, &name))
		return;

	struct ls_ctx lsctx = (struct ls_ctx){
		.ctx.path = ctx->ctx.meta.path ?: "",
		.ctx.name = name,
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
		return;
	}
	cp_pack_t pack = rpchandler_msg_new_response(&ctx->ctx);
	assert(pack);
	if (lsctx.ctx.name == NULL) {
		cp_pack_list_begin(pack);
		for (size_t i = 0; i < lsctx.strset.cnt; i++)
			cp_pack_str(pack, lsctx.strset.items[i].str);
		cp_pack_container_end(pack);
		shv_strset_free(&lsctx.strset);
	} else
		cp_pack_bool(pack, lsctx.located);
	cp_pack_container_end(pack);
	rpchandler_msg_send(&ctx->ctx);
}

static void handle_dir(struct msg_ctx *ctx) {
	char *name;
	if (!common_ls_dir(ctx, &name))
		return;
	if (!valid_path(ctx->handler, ctx->ctx.meta.path)) {
		rpchandler_msg_send_error(
			&ctx->ctx, RPCERR_METHOD_NOT_FOUND, "No such node");
		return;
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
}

static void handle_msg(struct msg_ctx *ctx) {
	for (const struct rpchandler_stage *s = ctx->handler->stages; s->funcs; s++)
		if (s->funcs->msg && s->funcs->msg(s->cookie, &ctx->ctx))
			return;
	if (!rpchandler_msg_valid(&ctx->ctx))
		return;
	if (ctx->ctx.meta.type == RPCMSG_T_REQUEST)
		rpchandler_msg_send_method_not_found(&ctx->ctx);
}

bool rpchandler_next(struct rpchandler *handler) {
	pthread_mutex_lock(&handler->lock);
	if (rpcclient_nextmsg(handler->client)) {
		void *obs_base = obstack_base(&handler->obstack);
		struct cpitem item;
		cpitem_unpack_init(&item);
		struct rpcmsg_meta meta;

		cp_unpack_t unpack = rpcclient_unpack(handler->client);
		if (rpcmsg_head_unpack(unpack, &item, &meta, NULL, &handler->obstack)) {
			struct msg_ctx ctx = {
				.ctx.meta = meta,
				.ctx.unpack = unpack,
				.ctx.item = &item,
				.handler = handler,
			};
			if (meta.type == RPCMSG_T_REQUEST && !strcmp(meta.method, "ls"))
				handle_ls(&ctx);
			else if (meta.type == RPCMSG_T_REQUEST && !strcmp(meta.method, "dir"))
				handle_dir(&ctx);
			else
				handle_msg(&ctx);
			clock_gettime(CLOCK_MONOTONIC, &handler->last_receive);
		}
		obstack_free(&handler->obstack, obs_base);
	}
	pthread_mutex_unlock(&handler->lock);
	return rpcclient_connected(handler->client);
}

int rpchandler_idling(rpchandler_t handler) {
	pthread_mutex_lock(&handler->lock);
	struct idle_ctx ctx = {
		.ctx.last_send = handler->last_send,
		.handler = handler,
		.msg_sent = false,
	};
	int res = INT_MAX;
	for (const struct rpchandler_stage *s = handler->stages; s->funcs; s++) {
		if (s->funcs->idle) {
			int t = s->funcs->idle(s->cookie, &ctx.ctx);
			if (res > t)
				res = t;
		}
	}
	pthread_mutex_unlock(&handler->lock);
	return res;
}

void rpchandler_run(rpchandler_t handler, volatile sig_atomic_t *halt) {
	while ((!halt || !*halt) && rpcclient_connected(handler->client)) {
		// TODO attempt client reset when that is implemented
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


void rpchandler_ls_result(struct rpchandler_ls *ctx, const char *name) {
	struct ls_ctx *lsctx = (struct ls_ctx *)ctx;
	if (ctx->name)
		lsctx->located = lsctx->located || !strcmp(ctx->name, name);
	else
		shv_strset_add(&lsctx->strset, name);
}
void rpchandler_ls_result_const(struct rpchandler_ls *ctx, const char *name) {
	struct ls_ctx *lsctx = (struct ls_ctx *)ctx;
	if (ctx->name)
		lsctx->located = lsctx->located || !strcmp(ctx->name, name);
	else
		shv_strset_add_const(&lsctx->strset, name);
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
	if (ctx->name) {
		if (lsctx->located)
			return;
		va_list cargs;
		va_copy(cargs, args);
		// https://github.com/llvm/llvm-project/issues/40656
		int siz = vsnprintf( // NOLINT(clang-analyzer-valist.Uninitialized)
			NULL, 0, fmt, cargs);
		va_end(cargs);
		if (siz != strlen(ctx->name)) {
			return;
		}
		char str[siz];
		assert(vsnprintf(str, siz, fmt, args) == siz);
		lsctx->located = !strcmp(ctx->name, str);
	} else {
		char *str;
		assert(vasprintf(&str, fmt, args) > 0);
		shv_strset_add_dyn(&lsctx->strset, str);
	}
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


struct obstack *_rpchandler_obstack(rpchandler_t rpchandler) {
	return &rpchandler->obstack;
}
