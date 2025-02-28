/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCALERTS_H
#define SHV_RPCALERTS_H
/*! @file
 * This file provides functions and definitions used to pack and unpack
 * alerts method.
 */

#include <shv/rpchandler.h>

/*! This defines the start level for notice alerts. */
#define RPCALERTS_NOTICE_FIRST 0
/*! This defines the start level for warning alerts. */
#define RPCALERTS_WARNING_FIRST 21
/*! This defines the start level for error alerts. */
#define RPCALERTS_ERROR_FIRST 43

/*! File nodes access levels */
enum rpcalerts_keys {
	RPCALERTS_KEY_DATE = 0,
	RPCALERTS_KEY_LEVEL = 1,
	RPCALERTS_KEY_ID = 2,
	RPCALERTS_KEY_INFO = 3,
};

/*! This structure represents one alert on a device. */
struct rpcalerts {
	/*! Date and time of the alert creation stored as the number of miliseconds
	 * since the Epoch
	 */
	uint64_t time;
	/*! This is value between 0 and 63 that can be used to sort alerts.*/
	uint8_t level;
	/*! String with notice identifier.
	 *
	 * This identifier should be chosen to be unique not for a single
	 * device but also between others. It should be short but at the same time
	 * precise and unique. The benefit is human readability.
	 */
	const char *id;
};

/*! Pack alert.
 *
 * @param pack The generic packer where it is about to be packed.
 * @param alert The pointer to @ref rpcalerts structure.
 * @returns `false` if packing encounters failure and `true` otherwise.
 */
[[gnu::nonnull]]
bool rpcalerts_pack(cp_pack_t pack, const struct rpcalerts *alert);

/*! Unpack one alert.
 *
 * @param unpack The generic unpacker to be used to unpack items.
 * @param item The item instance that was used to unpack the previous item.
 * @param obstack Pointer to the obstack instance to be used to allocate
 *   the returned alert.
 * @returns Pointer to the @ref rpcalerts structure or `NULL` in case of
 * an unpack error. You can investigate `item` to identify the failure cause.
 */
[[gnu::nonnull]]
struct rpcalerts *rpcalerts_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);

#endif /* SHV_RPCALERTS_H */
