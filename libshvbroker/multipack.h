#ifndef SHVBROKER_MULTIPACK_H
#define SHVBROKER_MULTIPACK_H

#include <shv/cp_pack.h>
#include "broker.h"

struct multipack {
	cp_pack_func_t func;
	cp_pack_t *packs;
	size_t cnt;
};

nbool_t signal_destinations(struct rpcbroker *broker, const char *path,
	const char *source, const char *signal, rpcaccess_t access)
	__attribute__((nonnull));

cp_pack_t multipack_init(struct rpcbroker *broker, struct multipack *multipack,
	nbool_t destinations) __attribute__((nonnull(1, 2)));

void multipack_done(struct rpcbroker *broker, struct multipack *multipack,
	nbool_t destinations, bool send) __attribute__((nonnull(1, 2)));

#endif
