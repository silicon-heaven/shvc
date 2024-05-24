#include <shv/rpchandler.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <semaphore.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include "strset.h"

struct rpchandler {
	struct rpcreceive recv;

	const struct rpchandler_stage *_Atomic stages;
	const struct rpcmsg_meta_limits *_Atomic meta_limits;

	rpcclient_t client;
	/* Allow sending messages only after requests.
	 * Sending in the receive thread can very easily result in deadlock if
	 * two SHVC handlers are put against each other. On the other hand it is
	 * very convenient to immediately respond to the request and thus we do
	 * a compromise, we allow sending messages after request and not after
	 * responses and signals.
	 */
	bool can_send;
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

	/* We use two mutexes to implement priority locking. The thread
	 * receiving messages needs to have priority over others and thus it has
	 * a dedicated lock. Regular locks need to check if they can lock the
	 * prioprity lock and just release the regular lock if they can't. The
	 * priority thread takes priority lock to force others to yield the
	 * primary lock.
	 */
	pthread_mutex_t mutex;
	_Atomic bool priority_lock;

	int nextrid;
};

struct rpchandler_x_ctx {
	char *name;
	cp_pack_t pack;
	bool located;
};

struct rpchandler_ls_ctx {
	struct rpchandler_x_ctx x;
	struct strset strset;
};

struct rpchandler_dir_ctx {
	struct rpchandler_x_ctx x;
};

rpchandler_t rpchandler_new(rpcclient_t client,
	const struct rpchandler_stage *stages,
	const struct rpcmsg_meta_limits *limits) {
	struct rpchandler *res = malloc(sizeof *res);
	res->stages = stages;
	res->meta_limits = limits;

	res->client = client;
	clock_gettime(CLOCK_MONOTONIC, &res->last_receive);
	clock_gettime(CLOCK_MONOTONIC, &res->last_send);
	pthread_mutex_init(&res->mutex, NULL);
	res->priority_lock = false;

	/* There can be login in front of this which uses 1-3 as request ID.
	 * Technically we could use here even 1, because those requests are already
	 * handled and thus no collision can happen, but let's not.
	 */
	res->nextrid = 4;
	obstack_init(&res->recv.obstack);
	return res;
}

void rpchandler_destroy(rpchandler_t rpchandler) {
	if (rpchandler == NULL)
		return;
	obstack_free(&rpchandler->recv.obstack, NULL);
	pthread_mutex_destroy(&rpchandler->mutex);
	free(rpchandler);
}

static void priority_lock(rpchandler_t rpchandler) {
	rpchandler->priority_lock = true;
	pthread_mutex_lock(&rpchandler->mutex);
	rpchandler->priority_lock = false;
}

static void lock(rpchandler_t rpchandler) {
	while (true) {
		pthread_mutex_lock(&rpchandler->mutex);
		if (!rpchandler->priority_lock)
			break;
		pthread_mutex_unlock(&rpchandler->mutex);
		sched_yield(); /* Just so we would not try to lock it immediatelly */
	}
}

static void unlock(rpchandler_t rpchandler) {
	pthread_mutex_unlock(&rpchandler->mutex);
}

static bool parse_ls_dir(struct rpcreceive *recv, struct rpcmsg_meta *meta,
	struct rpchandler_x_ctx *ctx) {
	ctx->name = NULL;
	ctx->pack = NULL;
	bool invalid_param = false;
	if (recv->item.type != CPITEM_CONTAINER_END) {
		cp_unpack(recv->unpack, &recv->item);
		switch (recv->item.type) {
			case CPITEM_NULL:
			case CPITEM_LIST: /* TODO backward compatible */
				break;
			case CPITEM_STRING:
				ctx->name =
					cp_unpack_strdupo(recv->unpack, &recv->item, &recv->obstack);
				break;
			default:
				invalid_param = true;
				break;
		}
	}
	if (recv->item.type == CPITEM_INVALID && recv->item.as.Error == CPERR_IO)
		return false;
	if (!rpcreceive_validmsg(recv))
		return false;

	cp_pack_t pack = rpcreceive_msg_new(recv);
	if (invalid_param) {
		rpcmsg_pack_error(pack, meta, RPCMSG_E_INVALID_PARAMS,
			"Use Null or String with node name");
		rpcreceive_msg_send(recv);
		return false;
	} else
		ctx->pack = pack;
	return true;
}

static void handle_ls(const struct rpchandler_stage *stages,
	struct rpcreceive *recv, struct rpcmsg_meta *meta) {
	struct rpchandler_ls_ctx ctx;
	if (!parse_ls_dir(recv, meta, &ctx.x))
		return;

	ctx.x.located = false;
	ctx.strset = (struct strset){};
	for (const struct rpchandler_stage *s = stages; s->funcs; s++)
		if (s->funcs->ls)
			s->funcs->ls(s->cookie, meta->path, &ctx);
	rpcmsg_pack_response(ctx.x.pack, meta);
	if (ctx.x.name == NULL) {
		cp_pack_list_begin(ctx.x.pack);
		for (size_t i = 0; i < ctx.strset.cnt; i++) {
			cp_pack_str(ctx.x.pack, ctx.strset.items[i].str);
		}
		cp_pack_container_end(ctx.x.pack);
		shv_strset_free(&ctx.strset);
	} else
		cp_pack_bool(ctx.x.pack, ctx.x.located);
	cp_pack_container_end(ctx.x.pack);
	rpcreceive_msg_send(recv);
}

static void handle_dir(const struct rpchandler_stage *stages,
	struct rpcreceive *recv, struct rpcmsg_meta *meta) {
	struct rpchandler_dir_ctx ctx;
	if (!parse_ls_dir(recv, meta, &ctx.x))
		return;

	ctx.x.located = false;
	rpcmsg_pack_response(ctx.x.pack, meta);
	if (ctx.x.name == NULL)
		cp_pack_list_begin(ctx.x.pack);
	rpchandler_dir_result(
		&ctx, "dir", RPCNODE_DIR_RET_PARAM, 0, RPCMSG_ACC_BROWSE, NULL);
	rpchandler_dir_result(
		&ctx, "ls", RPCNODE_DIR_RET_PARAM, 0, RPCMSG_ACC_BROWSE, NULL);

	if (meta->path == NULL)
		meta->path = "";

	for (const struct rpchandler_stage *s = stages; s->funcs; s++)
		if (s->funcs->dir)
			s->funcs->dir(s->cookie, meta->path, &ctx);
	if (ctx.x.name == NULL)
		cp_pack_container_end(ctx.x.pack);
	else if (!ctx.x.located)
		cp_pack_null(ctx.x.pack);
	cp_pack_container_end(ctx.x.pack);
	rpcreceive_msg_send(recv);
}

static void handle_msg(const struct rpchandler_stage *stages,
	struct rpcreceive *recv, struct rpcmsg_meta *meta) {
	const struct rpchandler_stage *s = stages;
	while (s->funcs && !s->funcs->msg(s->cookie, recv, meta))
		s++;
	if (s->funcs == NULL && meta->type == RPCMSG_T_REQUEST) {
		/* Default handler for requests, other types are dropped. */
		if (!rpcreceive_validmsg(recv))
			return;
		cp_pack_t pack = rpcreceive_msg_new(recv);
		rpcmsg_pack_ferror(pack, meta, RPCMSG_E_METHOD_NOT_FOUND,
			"No such method '%s' on path '%s'", meta->method, meta->path);
		rpcreceive_msg_send(recv);
	}
}

bool rpchandler_next(struct rpchandler *rpchandler) {
	if (!(rpcclient_nextmsg(rpchandler->client)))
		return false;

	void *obs_base = obstack_base(&rpchandler->recv.obstack);
	cpitem_unpack_init(&rpchandler->recv.item);
	struct rpcmsg_meta meta;
	if (rpcmsg_head_unpack(rpcclient_unpack(rpchandler->client),
			&rpchandler->recv.item, &meta, NULL, &rpchandler->recv.obstack)) {
		rpchandler->recv.unpack = rpcclient_unpack(rpchandler->client);
		rpchandler->can_send = meta.type == RPCMSG_T_REQUEST;
		if (meta.method && !strcmp(meta.method, "ls"))
			handle_ls(rpchandler->stages, &rpchandler->recv, &meta);
		else if (meta.method && !strcmp(meta.method, "dir"))
			handle_dir(rpchandler->stages, &rpchandler->recv, &meta);
		else
			handle_msg(rpchandler->stages, &rpchandler->recv, &meta);
	}

	obstack_free(&rpchandler->recv.obstack, obs_base);
	clock_gettime(CLOCK_MONOTONIC, &rpchandler->last_receive);
	return rpcclient_connected(rpchandler->client);
}

/* Calculate the maximum sleep before we need to send ping. */
static int maxsleep(rpchandler_t handler) {
	priority_lock(handler);
	struct timespec t;
	assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
	// TODO make idle time configurable
	/* The seconds precission is enough for us */
	int res = (RPC_DEFAULT_IDLE_TIME / 2) - t.tv_sec + handler->last_send.tv_sec;
	unlock(handler);
	return res > 0 ? (res * 1000) : 0;
}

void rpchandler_run(rpchandler_t rpchandler,
	void (*onerr)(rpchandler_t, rpcclient_t, enum rpchandler_error)) {
	while (rpcclient_connected(rpchandler->client)) {
		while (true) {
			struct pollfd pfd = {
				.fd = rpcclient_pollfd(rpchandler->client),
				.events = POLLIN | POLLHUP,
			};
			int pr = poll(&pfd, 1, maxsleep(rpchandler));
			if (pr == 0) { /* Timeout */
				priority_lock(rpchandler);
				if (onerr)
					onerr(rpchandler, rpchandler->client, RPCHANDLER_TIMEOUT);
				else {
					rpcmsg_pack_request_void(
						rpcclient_pack(rpchandler->client), ".app", "ping", 0);
					rpcclient_sendmsg(rpchandler->client);
					clock_gettime(CLOCK_MONOTONIC, &rpchandler->last_send);
				}
				unlock(rpchandler);
			} else if (pr == -1 || pfd.revents & POLLERR) {
				fprintf(stderr, "Poll error: %s\n", strerror(errno));
				abort(); // TODO
			} else if (pfd.revents & (POLLIN | POLLHUP)) {
				/* Even on pollhup we request so RPC Client actually detest the
				 * disconnect not just us.
				 */
				if (!rpchandler_next(rpchandler))
					break;
			}
		}
		if (onerr)
			onerr(rpchandler, rpchandler->client, RPCHANDLER_DISCONNECT);
	}
}

struct thread_ctx {
	rpchandler_t handler;
	void (*onerr)(rpchandler_t, rpcclient_t, enum rpchandler_error);
};

static void *thread_loop(void *_ctx) {
	struct thread_ctx *ctx = _ctx;
	rpchandler_t handler = ctx->handler;
	void (*onerr)(rpchandler_t, rpcclient_t, enum rpchandler_error) = ctx->onerr;
	free(ctx);
	rpchandler_run(handler, onerr);
	return NULL;
}

int rpchandler_spawn_thread(rpchandler_t rpchandler,
	void (*onerr)(rpchandler_t, rpcclient_t, enum rpchandler_error),
	// TODO set higher priority to this thread than parent has. This is to
	// hopefully win all locks withut having other threads to release it when
	// this one waits for it.
	pthread_t *restrict thread, const pthread_attr_t *restrict attr) {
	struct thread_ctx *ctx = malloc(sizeof *ctx);
	*ctx = (struct thread_ctx){.handler = rpchandler, .onerr = onerr};
	return pthread_create(thread, attr, thread_loop, ctx);
}

int rpchandler_next_request_id(rpchandler_t rpchandler) {
	return rpchandler->nextrid++;
}


cp_pack_t rpchandler_msg_new(rpchandler_t rpchandler) {
	lock(rpchandler);
	return rpcclient_pack(rpchandler->client);
}

cp_pack_t rpchandler_msg_new_request(rpchandler_t rpchandler, const char *path,
	const char *method, int request_id) {
	cp_pack_t res = rpchandler_msg_new(rpchandler);
	if (!rpcmsg_pack_request(res, path, method, request_id)) {
		rpchandler_msg_drop(rpchandler);
		return NULL;
	}
	return res;
}

bool rpchandler_msg_new_request_void(rpchandler_t rpchandler, const char *path,
	const char *method, int request_id) {
	cp_pack_t pack = rpchandler_msg_new(rpchandler);
	if (!rpcmsg_pack_request_void(pack, path, method, request_id)) {
		rpchandler_msg_drop(rpchandler);
		return false;
	}
	return true;
}

bool rpchandler_msg_send(rpchandler_t rpchandler) {
	bool res = rpcclient_sendmsg(rpchandler->client);
	clock_gettime(CLOCK_MONOTONIC, &rpchandler->last_send);
	unlock(rpchandler);
	return res;
}

bool rpchandler_msg_drop(rpchandler_t rpchandler) {
	bool res = rpcclient_dropmsg(rpchandler->client);
	unlock(rpchandler);
	return res;
}


bool rpcreceive_validmsg(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	receive->unpack = NULL;
	return rpcclient_validmsg(rpchandler->client);
}

cp_pack_t rpcreceive_msg_new(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	if (receive->unpack != NULL || !rpchandler->can_send)
		return NULL;
	priority_lock(rpchandler);
	return rpcclient_pack(rpchandler->client);
}

bool rpcreceive_msg_send(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	if (!rpchandler->can_send)
		return false;
	bool res = rpcclient_sendmsg(rpchandler->client);
	clock_gettime(CLOCK_MONOTONIC, &rpchandler->last_send);
	unlock(rpchandler);
	return res;
}

bool rpcreceive_msg_drop(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	if (!rpchandler->can_send)
		return false;
	bool res = rpcclient_dropmsg(rpchandler->client);
	unlock(rpchandler);
	return res;
}

void rpchandler_ls_result(struct rpchandler_ls_ctx *ctx, const char *name) {
	if (ctx->x.name)
		ctx->x.located = !strcmp(ctx->x.name, name);
	else
		shv_strset_add(&ctx->strset, name);
}

void rpchandler_ls_const(struct rpchandler_ls_ctx *ctx, const char *name) {
	if (ctx->x.name)
		ctx->x.located = !strcmp(ctx->x.name, name);
	else
		shv_strset_add_const(&ctx->strset, name);
}

void rpchandler_ls_result_fmt(
	struct rpchandler_ls_ctx *context, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	rpchandler_ls_result_vfmt(context, fmt, args);
	va_end(args);
}

void rpchandler_ls_result_vfmt(
	struct rpchandler_ls_ctx *ctx, const char *fmt, va_list args) {
	if (ctx->x.name) {
		va_list cargs;
		va_copy(cargs, args);
		int siz = snprintf(NULL, 0, fmt, cargs);
		va_end(cargs);
		if (siz != strlen(ctx->x.name)) {
			va_end(args);
			return;
		}
		char str[siz];
		assert(snprintf(str, siz, fmt, args) == siz);
		ctx->x.located = !strcmp(ctx->x.name, str);
	} else {
		char *str;
		assert(vasprintf(&str, fmt, args) > 0);
		shv_strset_add_dyn(&ctx->strset, str);
	}
}

void rpchandler_dir_result(struct rpchandler_dir_ctx *ctx, const char *name,
	enum rpcnode_dir_signature signature, int flags, enum rpcmsg_access access,
	const char *description) {
	if (ctx->x.name) {
		if (strcmp(ctx->x.name, name))
			return;
		ctx->x.located = true;
	}
	cp_pack_map_begin(ctx->x.pack);
	cp_pack_str(ctx->x.pack, "name");
	cp_pack_str(ctx->x.pack, name);
	cp_pack_str(ctx->x.pack, "signature");
	cp_pack_int(ctx->x.pack, signature);
	cp_pack_str(ctx->x.pack, "flags");
	cp_pack_int(ctx->x.pack, flags);
	cp_pack_str(ctx->x.pack, "access");
	cp_pack_str(ctx->x.pack, rpcmsg_access_str(access));
	if (description) {
		cp_pack_str(ctx->x.pack, "description");
		cp_pack_str(ctx->x.pack, description);
	}
	cp_pack_container_end(ctx->x.pack);
}
