#ifndef SHV_RPCHANDLER_RESPONSES_H
#define SHV_RPCHANDLER_RESPONSES_H

#include <shv/rpchandler.h>


typedef struct rpchandler_responses *rpchandler_responses_t;

void rpchandler_responses_destroy(rpchandler_responses_t responder);

rpchandler_responses_t rpchandler_responses_new(void)
	__attribute__((malloc, malloc(rpchandler_responses_destroy, 1)));

struct rpchandler_stage rpchandler_responses_stage(
	rpchandler_responses_t responder) __attribute__((nonnull));

typedef struct rpcresponse *rpcresponse_t;

rpcresponse_t rpcresponse_expect(rpchandler_responses_t responder, int request_id)
	__attribute__((nonnull));

bool rpcresponse_waitfor(rpcresponse_t respond, struct rpcreceive **receive,
	struct rpcmsg_meta **meta, int timeout) __attribute__((nonnull));

bool rpcresponse_validmsg(rpcresponse_t respond) __attribute__((nonnull));


#endif
