#ifndef SHVBROKER_MOUNT_H
#define SHVBROKER_MOUNT_H

#include "broker.h"

[[gnu::nonnull]]
bool mount_register(struct clientctx *clientctx);

[[gnu::nonnull]]
void mount_unregister(struct clientctx *clientctx);

#endif
