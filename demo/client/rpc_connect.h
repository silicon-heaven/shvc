#ifndef _DEMO_RPC_CONNECTION_H
#define _DEMO_RPC_CONNECTION_H
#include <shv/rpcclient.h>
#include "opts.h"

rpcclient_t rpc_connect(const struct conf *conf) __attribute__((nonnull));

#endif
