/* SPDX-License-Identifier: MIT */
#ifndef SHV_SHA1_H
#define SHV_SHA1_H
/*! @file
 * SHA1 algorithm.
 *
 * This is not implemented by this library. Based on the compilation options of
 * the libshvrpc some third-party implementation is used instead. Thus this only
 * provides unified API for the SHA1 digest calculation.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*! Number of bytes for SHA1 hash. */
#define SHA1_SIZ (20)
/*! Number of characters for SHA1 hexadecimal representation. */
#define SHA1_HEX_SIZ (SHA1_SIZ * 2)

/*! SHA1 digest calculation context. */
typedef struct sha1ctx *sha1ctx_t;

/*! Create new context for SHA1 digest calculation.
 *
 * @returns The SHA1 digest calculation context.
 */
sha1ctx_t sha1_new(void);

/*! Destroy SHA1 digest calculation context.
 *
 * @param ctx The SHA1 calculation context to be destroyed (can be `NULL`).
 */
void sha1_destroy(sha1ctx_t ctx);

/*! Update SHA1 digest with provided bytes.
 *
 * @param ctx The SHA1 calculation context.
 * @param buf Pointer to the bytes hash should be updated with.
 * @param siz Number of bytes to be used.
 * @returns Boolean signaling if digest update was successful.
 */
bool sha1_update(sha1ctx_t ctx, const uint8_t *buf, size_t siz)
	__attribute__((nonnull));

/*! Receive SHA1 binary digest.
 *
 * @param ctx The SHA1 digest calculation context.
 * @param digest The array where digest bytes will be stored.
 * @returns Boolean signaling if digest was generated successfully.
 */
bool sha1_digest(sha1ctx_t ctx, uint8_t digest[SHA1_SIZ]);

/*! Receive SHA1 HEX digest.
 *
 * @param ctx The SHA1 digest calculation context.
 * @param digest The array where digest hex characters will be stored.
 * @returns Boolean signaling if digest was generated successfully.
 */
static inline bool sha1_hex_digest(sha1ctx_t ctx, char digest[SHA1_HEX_SIZ]) {
	uint8_t buf[SHA1_SIZ];
	if (!sha1_digest(ctx, buf))
		return false;
	static const char *const hex = "0123456789abcdef";
	for (unsigned i = 0; i < 20; i++) {
		*digest++ = hex[(buf[i] >> 4) & 0xf];
		*digest++ = hex[buf[i] & 0xf];
	}
	return true;
}

#endif
