#ifndef SHVBROKER_API_LOGIN_H
#define SHVBROKER_API_LOGIN_H

#include "broker.h"

[[gnu::nonnull(2)]]
void rpcbroker_api_login_msg(struct clientctx *c, struct rpchandler_msg *ctx);

#endif
