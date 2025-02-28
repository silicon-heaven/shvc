#ifndef SHVBROKER_API_H
#define SHVBROKER_API_H

#include "broker.h"

[[gnu::nonnull(2)]]
void rpcbroker_api_ls(struct clientctx *c, struct rpchandler_ls *ctx);
[[gnu::nonnull(1)]]
bool rpcbroker_api_dir(struct rpchandler_dir *ctx);
[[gnu::nonnull(2)]]
bool rpcbroker_api_msg(struct clientctx *c, struct rpchandler_msg *ctx);

#endif
