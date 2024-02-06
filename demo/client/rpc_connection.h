#ifndef _DEMO_RPC_CONNECTION_H
#define _DEMO__RPC_CONNECTION_H
#include <stdbool.h>
#include "opts.h"
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>

bool parse_rpcurl(const char *url, struct rpcurl **out_rpcurl);
bool connect_to_rpcurl(const struct rpcurl *rpcurl, rpcclient_t *out_client);
bool login_with_rpcclient(rpcclient_t client, const struct rpcurl *rpcurl);

#endif
