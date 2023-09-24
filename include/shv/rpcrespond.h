#ifndef SHV_RPCRESPOND_H
#define SHV_RPCRESPOND_H

#include <stdbool.h>
#include <pthread.h>
#include <shv/rpchandler.h>


typedef struct rpcresponder *rpcresponder_t;

void rpcresponder_destroy(rpcresponder_t responder);

rpcresponder_t rpcresponder_new(void)
	__attribute__((malloc, malloc(rpcresponder_destroy, 1)));

struct rpchandler_stage rpcresponder_handler_stage(rpcresponder_t responder)
	__attribute__((nonnull));

typedef struct rpcrespond *rpcrespond_t;

rpcrespond_t rpcrespond_expect(rpcresponder_t responder, int request_id)
	__attribute__((nonnull));

bool rpcrespond_waitfor(rpcrespond_t respond, struct rpcreceive **receive,
	struct rpcmsg_meta **meta, int timeout) __attribute__((nonnull));

bool rpcrespond_validmsg(rpcrespond_t respond) __attribute__((nonnull));


#endif
