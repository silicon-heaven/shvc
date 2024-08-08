#ifndef SHVBROKER_MOUNT_H
#define SHVBROKER_MOUNT_H

#include "broker.h"

bool mount_register(struct clientctx *clientctx) __attribute__((nonnull));

void mount_unregister(struct clientctx *clientctx) __attribute__((nonnull));

#endif
