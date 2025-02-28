#ifndef _SHVCHAINPACK_COMMON_H
#define _SHVCHAINPACK_COMMON_H

#include <shv/cp.h>

/* Common handling of the item for unpack functions.
 *
 * This covers initial sanity checks.
 */
[[gnu::nonnull]]
bool common_unpack(size_t *res, FILE *f, struct cpitem *item);

/* Common handling of the item for pack functions.
 *
 * This covers invalid item and error on file stream.
 */
[[gnu::nonnull(1, 3)]]
bool common_pack(ssize_t *res, FILE *f, const struct cpitem *item);

#endif
