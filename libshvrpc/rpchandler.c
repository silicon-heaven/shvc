#include <shv/rpchandler.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include "strset.h"

struct rpchandler {
	struct rpcreceive recv;

	const struct rpchandler_stage *stages;
	const struct rpcmsg_meta_limits *meta_limits;

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
	 * a dedicated lock. Regular locks need to check if they can lock the
	 * prioprity lock and just release the regular lock if they can't. The
	 * priority thread takes priority lock to force others to yield the
	 * primary lock.
	 */
	pthread_mutex_t mutex, pmutex;

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
	pthread_mutex_init(&res->mutex, NULL);
	pthread_mutex_init(&res->pmutex, NULL);

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
	pthread_mutex_destroy(&rpchandler->pmutex);
	free(rpchandler);
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
		return true;

	cp_pack_t pack = rpcreceive_response_new(recv);
	if (invalid_param) {
		rpcmsg_pack_error(pack, meta, RPCMSG_E_INVALID_PARAMS,
			"Use Null or String with node name");
		rpcreceive_response_send(recv);
		return false;
	} else
		ctx->pack = pack;
	return true;
}

static bool handle_ls(const struct rpchandler_stage *stages,
	struct rpcreceive *recv, struct rpcmsg_meta *meta) {
	struct rpchandler_ls_ctx ctx;
	printf("Wtf\n");
	if (!parse_ls_dir(recv, meta, &ctx.x))
		return false;
	if (ctx.x.pack == NULL)
		return true;

	ctx.x.located = false;
	ctx.strset = (struct strset){};
	for (const struct rpchandler_stage *s = stages; s->funcs; s++)
		if (s->funcs->ls)
			s->funcs->ls(s->cookie, meta->path, &ctx);
	printf("Before response pack\n");
	rpcmsg_pack_response(ctx.x.pack, meta);
	printf("Before packing\n");
	if (ctx.x.name == NULL) {
		cp_pack_list_begin(ctx.x.pack);
		for (size_t i = 0; i < ctx.strset.cnt; i++) {
			printf("Packing %s\n", ctx.strset.items[i].str);
			cp_pack_str(ctx.x.pack, ctx.strset.items[i].str);
		}
		cp_pack_container_end(ctx.x.pack);
		shv_strset_free(&ctx.strset);
	} else
		cp_pack_bool(ctx.x.pack, ctx.x.located);
	cp_pack_container_end(ctx.x.pack);
	rpcreceive_response_send(recv);
	return true;
}

static bool handle_dir(const struct rpchandler_stage *stages,
	struct rpcreceive *recv, struct rpcmsg_meta *meta) {
	struct rpchandler_dir_ctx ctx;
	if (!parse_ls_dir(recv, meta, &ctx.x))
		return false;

	ctx.x.located = false;
	rpcmsg_pack_response(ctx.x.pack, meta);
	if (ctx.x.name == NULL)
		cp_pack_list_begin(ctx.x.pack);
	rpchandler_dir_result(
		&ctx, "dir", RPCNODE_DIR_RET_PARAM, 0, RPCMSG_ACC_BROWSE, NULL);
	rpchandler_dir_result(
		&ctx, "ls", RPCNODE_DIR_RET_PARAM, 0, RPCMSG_ACC_BROWSE, NULL);
	for (const struct rpchandler_stage *s = stages; s->funcs; s++)
		if (s->funcs->dir)
			s->funcs->dir(s->cookie, meta->path, &ctx);
	if (ctx.x.name == NULL)
		cp_pack_container_end(ctx.x.pack);
	else if (!ctx.x.located)
		cp_pack_null(ctx.x.pack);
	cp_pack_container_end(ctx.x.pack);
	rpcreceive_response_send(recv);
	return true;
}

static bool handle_msg(const struct rpchandler_stage *stages,
	struct rpcreceive *recv, struct rpcmsg_meta *meta) {
	enum rpchandler_func_res fres = RPCHFR_UNHANDLED;
	for (const struct rpchandler_stage *s = stages;
		 s->funcs && fres == RPCHFR_UNHANDLED; s++)
		fres = s->funcs->msg(s->cookie, recv, meta);
	if (fres == RPCHFR_UNHANDLED) {
		/* Default handler for requests, other types are dropped. */
		if (!rpcreceive_validmsg(recv))
			return false;
		if (meta->type == RPCMSG_T_REQUEST) {
			cp_pack_t pack = rpcreceive_response_new(recv);
			rpcmsg_pack_ferror(pack, meta, RPCMSG_E_METHOD_NOT_FOUND,
				"No such method '%s' on path '%s'", meta->method, meta->path);
			rpcreceive_response_send(recv);
		}
	}
	// TODO handle other fres
	return true;
}

bool rpchandler_next(struct rpchandler *rpchandler) {
	if (!(rpcclient_nextmsg(rpchandler->client)))
		return false;

	bool res = false;
	void *obs_base = obstack_base(&rpchandler->recv.obstack);
	cpitem_unpack_init(&rpchandler->recv.item);
	struct rpcmsg_meta meta;
	if (rpcmsg_head_unpack(rpcclient_unpack(rpchandler->client),
			&rpchandler->recv.item, &meta, NULL, &rpchandler->recv.obstack)) {
		rpchandler->recv.unpack = rpcclient_unpack(rpchandler->client);
		rpchandler->can_respond = meta.type == RPCMSG_T_REQUEST;
		if (!strcmp(meta.method, "ls"))
			res = handle_ls(rpchandler->stages, &rpchandler->recv, &meta);
		else if (!strcmp(meta.method, "dir"))
			res = handle_dir(rpchandler->stages, &rpchandler->recv, &meta);
		else
			res = handle_msg(rpchandler->stages, &rpchandler->recv, &meta);
	}

	obstack_free(&rpchandler->recv.obstack, obs_base);
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
		pthread_mutex_lock(&rpchandler->mutex);
		if (pthread_mutex_trylock(&rpchandler->pmutex) == 0) {
			pthread_mutex_unlock(&rpchandler->pmutex);
			break;
		} else
			pthread_mutex_unlock(&rpchandler->mutex);
	}
	return rpcclient_pack(rpchandler->client);
}

bool rpchandler_msg_send(rpchandler_t rpchandler) {
	bool res = rpcclient_sendmsg(rpchandler->client);
	pthread_mutex_unlock(&rpchandler->mutex);
	return res;
}

bool rpchandler_msg_drop(rpchandler_t rpchandler) {
	bool res = rpcclient_dropmsg(rpchandler->client);
	pthread_mutex_unlock(&rpchandler->mutex);
	return res;
}


bool rpcreceive_validmsg(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	receive->unpack = NULL;
	return rpcclient_validmsg(rpchandler->client);
}

cp_pack_t rpcreceive_response_new(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	/* DUe to the logging the message needs to be first read to release the lock
	 * for the logging before we start writing.
	 */
	assert(receive->unpack == NULL);
	assert(rpchandler->can_respond);
	pthread_mutex_lock(&rpchandler->pmutex);
	pthread_mutex_lock(&rpchandler->mutex);
	pthread_mutex_unlock(&rpchandler->pmutex);
	return rpcclient_pack(rpchandler->client);
}

bool rpcreceive_response_send(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	assert(rpchandler->can_respond);
	bool res = rpcclient_sendmsg(rpchandler->client);
	rpchandler->can_respond = false;
	pthread_mutex_unlock(&rpchandler->mutex);
	return res;
}

bool rpcreceive_response_drop(struct rpcreceive *receive) {
	struct rpchandler *rpchandler = (struct rpchandler *)receive;
	assert(rpchandler->can_respond);
	bool res = rpcclient_dropmsg(rpchandler->client);
	pthread_mutex_unlock(&rpchandler->mutex);
	return res;
}

void rpchandler_ls_result(struct rpchandler_ls_ctx *ctx, const char *name) {
	printf("LS with %s\n", name);
	if (ctx->x.name)
		ctx->x.located = !strcmp(ctx->x.name, name);
	else
		shv_strset_add(&ctx->strset, name);
	printf("LS with %s done\n", name);
}

void rpchandler_ls_const(struct rpchandler_ls_ctx *ctx, const char *name) {
	if (ctx->x.name)
		ctx->x.located = !strcmp(ctx->x.name, name);
	else
		shv_strset_add_const(&ctx->strset, name);
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

bool rpchandler_dir_result(struct rpchandler_dir_ctx *ctx, const char *name,
	enum rpcnode_dir_signature signature, int flags, enum rpcmsg_access access,
	const char *description) {
	if (ctx->x.name) {
		if (strcmp(ctx->x.name, name))
			return true;
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
	// TODO error?
	return true;
}
