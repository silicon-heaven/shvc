#ifndef _SHVCHAINPACK_COMMON_H
#define _SHVCHAINPACK_COMMON_H

#include <shv/cp.h>

/* Common handling of the item for unpack functions.
 *
 * This covers RAW access as well as initial sanity checks.
 */
bool common_unpack(size_t *res, FILE *f, struct cpitem *item);

/* Common handling of the item for pack functions.
 *
 * This covers RAW access and invalid item.
 */
bool common_pack(size_t *res, FILE *f, const struct cpitem *item);

#endif
