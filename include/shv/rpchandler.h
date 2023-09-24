#ifndef SHV_RPCHANDLER_H
#define SHV_RPCHANDLER_H

#include <stdbool.h>
#include <pthread.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include <shv/rpcnode.h>


struct rpcreceive {
	cp_unpack_t unpack;
	struct cpitem item;
	struct obstack obstack;
};

enum rpchandler_func_res {
	RPCHFR_UNHANDLED,
	RPCHFR_HANDLED,
	RPCHFR_RECV_ERR,
	RPCHFR_SEND_ERR,
};

struct rpchandler_ls_ctx;

struct rpchandler_dir_ctx;

struct rpchandler_funcs {
	enum rpchandler_func_res (*msg)(void *cookie, struct rpcreceive *receive,
		const struct rpcmsg_meta *meta);
	void (*ls)(void *cookie, const char *path, struct rpchandler_ls_ctx *ctx);
	void (*dir)(void *cookie, const char *path, struct rpchandler_dir_ctx *ctx);
};

struct rpchandler_stage {
	const struct rpchandler_funcs *funcs;
	void *cookie;
};

typedef struct rpchandler *rpchandler_t;


void rpchandler_destroy(rpchandler_t rpchandler);

rpchandler_t rpchandler_new(rpcclient_t client,
	const struct rpchandler_stage *stages, const struct rpcmsg_meta_limits *limits)
	__attribute__((nonnull(1), malloc, malloc(rpchandler_destroy, 1)));

void rpchandler_change_stages(rpchandler_t handler,
	const struct rpchandler_stage *stages) __attribute__((nonnull));

bool rpchandler_next(rpchandler_t rpchandler) __attribute__((nonnull));

int rpchandler_spawn_thread(rpchandler_t rpchandler, pthread_t *restrict thread,
	const pthread_attr_t *restrict attr) __attribute__((nonnull(1, 2)));


int rpchandler_next_request_id(rpchandler_t rpchandler) __attribute__((nonnull));


cp_pack_t rpchandler_msg_new(rpchandler_t rpchandler) __attribute__((nonnull));

bool rpchandler_msg_send(rpchandler_t rpchandler) __attribute__((nonnull));

bool rpchandler_msg_drop(rpchandler_t rpchandler) __attribute__((nonnull));


bool rpcreceive_validmsg(struct rpcreceive *receive) __attribute__((nonnull));

cp_pack_t rpcreceive_response_new(struct rpcreceive *receive)
	__attribute__((nonnull));

bool rpcreceive_response_send(struct rpcreceive *receive) __attribute__((nonnull));

bool rpcreceive_response_drop(struct rpcreceive *receive) __attribute__((nonnull));


bool rpchandler_ls_result(struct rpchandler_ls_ctx *context, const char *name)
	__attribute__((nonnull));

const char *rpchandler_ls_name(struct rpchandler_ls_ctx *context)
	__attribute__((nonnull));

bool rpchandler_dir_result(struct rpchandler_dir_ctx *context, const char *name,
	enum rpcnode_dir_signature signature, int flags, enum rpcmsg_access access,
	const char *description) __attribute__((nonnull(1, 2)));

const char *rpchandler_dir_name(struct rpchandler_ls_ctx *context)
	__attribute__((nonnull));

#endif
