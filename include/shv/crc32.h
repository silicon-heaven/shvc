/* SPDX-License-Identifier: MIT */
#ifndef SHV_CRC32_H
#define SHV_CRC32_H
/*! @file
 * CRC algorithm used in IEEE 802.3 and know as plain CRC-32.
 */

#include <stddef.h>
#include <stdint.h>

/*! The type definition for the CRC32 state value. */
typedef uint32_t crc32_t;

/*! Initialization value for the CRC32 state.
 *
 * @returns The initialization value for CRC32 state.
 */
static inline crc32_t crc32_init(void) {
	return 0xffffffff;
}

/*! Provide updated CRC value.
 *
 * This doesn't directly update the provided CRC state and instead returns the
 * new value. The common use case is `crc = crc32_update(crc, buf, len)`.
 *
 * @param crc The previous CRC state. For the initial calculation you should
 *   use @ref crc32_init.
 * @param data Pointer to the bytes CRC should be calculated for.
 * @param len Number of bytes CRC should be calculated for.
 * @returns The new CRC32 state.
 */
crc32_t crc32_update(crc32_t crc, uint8_t *data, size_t len);

/*! Provide updated CRC value from a single byte.
 *
 * This is utility function that wraps @ref crc32_update in such a way that it
 * can easilly be called with a single byte.
 *
 * @param crc The previous CRC state. For the initial calculation you should
 *   use @ref crc32_init.
 * @param byte The byte to be used to update CRC32 state.
 * @returns The new CRC32 state.
 */
static inline crc32_t crc32_cupdate(crc32_t crc, uint8_t byte) {
	return crc32_update(crc, &byte, 1);
}

/*! Finalize the CRC32 calculation.
 *
 * The state is not the CRC32 value as is, it must be finalized to get the
 * correct CRC-32 value.
 *
 * @param crc The accumulated CRC32 state.
 * @returns The CRC-32 value.
 */
static inline uint32_t crc32_finalize(const crc32_t crc) {
	return ~crc;
}

/*! Utility function for the CRC32 of the provided data.
 *
 * This combines @ref crc32_init, @ref crc32_update, and @ref crc32_finalize in
 * a single package.
 *
 * @param data Pointer to the bytes CRC should be calculated for.
 * @param len Number of bytes CRC should be calculated for.
 * @returns The calculated CRC-32 value.
 */
static inline uint32_t crc32(uint8_t *data, size_t len) {
	return crc32_finalize(crc32_update(crc32_init(), data, len));
}

#endif
