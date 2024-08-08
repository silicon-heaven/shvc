#ifndef SHVBROKER_API_LOGIN_H
#define SHVBROKER_API_LOGIN_H

#include "broker.h"

void rpcbroker_api_login_msg(struct clientctx *c, struct rpchandler_msg *ctx)
	__attribute__((nonnull(2)));

#endif
