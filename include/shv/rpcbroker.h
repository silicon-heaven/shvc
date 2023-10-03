/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCBROKER_H
#define SHV_RPCBROKER_H
/*! @file
 * Implementaion of RPC Broker based on RPC Handler.
 */

#include <stdbool.h>
#include <shv/rpchandler.h>

typedef struct rpcbroker *rpcbroker_t;


void rpcbroker_destroy(rpcbroker_t rpcbroker);

rpcbroker_t rpcbroker_new(void);


struct rpchandler_stage rpcbroker_handler_stage(rpcbroker_t rpcbroker);



#endif
