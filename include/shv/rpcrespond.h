#ifndef SHV_RPCRESPOND_H
#define SHV_RPCRESPOND_H

#include <stdbool.h>
#include <pthread.h>
#include <shv/rpchandler.h>


typedef struct rpcresponder *rpcresponder_t;

void rpcresponder_destroy(rpcresponder_t responder);

rpcresponder_t rpcresponder_new(void)
	__attribute__((malloc, malloc(rpcresponder_destroy, 1)));

rpchandler_func *rpcresponder_func(rpcresponder_t responder)
	__attribute__((nonnull, returns_nonnull));

typedef struct rpcrespond *rpcrespond_t;

rpcrespond_t rpcrespond_expect(rpcresponder_t responder, int request_id)
	__attribute__((nonnull));

bool rpcrespond_waitfor(rpcrespond_t respond, cp_unpack_t *unpack,
	struct cpitem **item, int timeout) __attribute__((nonnull));

const struct rpcmsg_meta *rpcrespond_meta(rpcrespond_t respond)
	__attribute__((nonnull));

bool rpcrespond_validmsg(rpcrespond_t respond) __attribute__((nonnull));


#endif
