/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_APP_H
#define SHV_RPCHANDLER_APP_H
/*! @file */

#include <shv/rpchandler.h>

typedef struct rpchandler_app *rpchandler_app_t;

void rpchandler_app_destroy(rpchandler_app_t rpchandler_app);

rpchandler_app_t rpchandler_app_new(const char *name, const char *version)
	__attribute__((malloc, malloc(rpchandler_app_destroy, 1)));

struct rpchandler_stage rpchandler_app_stage(rpchandler_app_t rpchandler_app)
	__attribute__((nonnull));


#endif
