#ifndef SHV_RPCAPP_H
#define SHV_RPCAPP_H

#include <shv/rpchandler.h>

typedef struct rpcapp *rpcapp_t;

void rpcapp_destroy(rpcapp_t rpcapp);

rpcapp_t rpcapp_new(const char *name, const char *version)
	__attribute__((malloc, malloc(rpcapp_destroy, 1)));

struct rpchandler_stage rpcapp_handler_stage(rpcapp_t rpcapp)
	__attribute__((nonnull));


#endif
