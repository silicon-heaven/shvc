#ifndef SHVBROKER_API_H
#define SHVBROKER_API_H

#include "broker.h"

void rpcbroker_api_ls(struct clientctx *c, struct rpchandler_ls *ctx)
	__attribute__((nonnull(2)));
bool rpcbroker_api_dir(struct rpchandler_dir *ctx) __attribute__((nonnull(1)));
bool rpcbroker_api_msg(struct clientctx *c, struct rpchandler_msg *ctx)
	__attribute__((nonnull(2)));

#endif
