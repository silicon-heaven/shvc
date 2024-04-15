#ifndef SHV_HASHSET_H
#define SHV_HASHSET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SHA1_HEX_SIZ 40

struct sha1ctx;
typedef struct sha1ctx *sha1ctx_t;

/*! Create new context for SHA1 calculation. */
sha1ctx_t sha1_new(void);

/*! Destroy SHA1 calculation context. */
void sha1_destroy(sha1ctx_t ctx);

/*! Update SHA1 with provided bytes.
 */
bool sha1_update(sha1ctx_t ctx, const uint8_t *buf, size_t siz);

/*! Receive SHA1 HEX digest.
 */
bool sha1_hex_digest(sha1ctx_t ctx, char digest[SHA1_HEX_SIZ]);

#endif
