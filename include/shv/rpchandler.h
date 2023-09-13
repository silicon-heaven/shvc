#ifndef SHV_RPCHANDLER_H
#define SHV_RPCHANDLER_H

#include <stdbool.h>
#include <pthread.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>


typedef struct rpchandler *rpchandler_t;
typedef struct rpcreceive *rpcreceive_t;

enum rpchandler_func_res {
	RPCHFR_UNHANDLED,
	RPCHFR_HANDLED,
	RPCHFR_RECV_ERR,
	RPCHFR_SEND_ERR,
};

typedef enum rpchandler_func_res (*rpchandler_func)(void *ptr, rpcreceive_t receive,
	cp_unpack_t unpack, struct cpitem *item, const struct rpcmsg_meta *meta);


void rpchandler_destroy(rpchandler_t rpchandler);

rpchandler_t rpchandler_new(rpcclient_t client, const rpchandler_func **funcs,
	const struct rpcmsg_meta_limits *limits)
	__attribute__((nonnull(1), malloc, malloc(rpchandler_destroy, 1)));


bool rpchandler_next(rpchandler_t rpchandler) __attribute__((nonnull));

int rpchandler_spawn_thread(rpchandler_t rpchandler, pthread_t *restrict thread,
	const pthread_attr_t *restrict attr) __attribute__((nonnull(1, 2)));


int rpchandler_next_request_id(rpchandler_t rpchandler) __attribute__((nonnull));


cp_pack_t rpchandler_msg_new(rpchandler_t rpchandler) __attribute__((nonnull));

bool rpchandler_msg_send(rpchandler_t rpchandler) __attribute__((nonnull));

bool rpchandler_msg_drop(rpchandler_t rpchandler) __attribute__((nonnull));


bool rpcreceive_validmsg(rpcreceive_t receive) __attribute__((nonnull));

cp_pack_t rpcreceive_response_new(rpcreceive_t receive) __attribute__((nonnull));

bool rpcreceive_response_send(rpcreceive_t receive) __attribute__((nonnull));

bool rpcreceive_response_drop(rpcreceive_t receive) __attribute__((nonnull));


#endif
