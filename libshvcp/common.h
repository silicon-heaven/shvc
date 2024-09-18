#ifndef _SHVCHAINPACK_COMMON_H
#define _SHVCHAINPACK_COMMON_H

#include <shv/cp.h>

/* Common handling of the item for unpack functions.
 *
 * This covers initial sanity checks.
 */
bool common_unpack(size_t *res, FILE *f, struct cpitem *item)
	__attribute__((nonnull));

/* Common handling of the item for pack functions.
 *
 * This covers invalid item and error on file stream.
 */
bool common_pack(ssize_t *res, FILE *f, const struct cpitem *item)
	__attribute__((nonnull(1, 3)));

#endif
