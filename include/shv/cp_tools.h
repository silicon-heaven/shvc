/* SPDX-License-Identifier: MIT */
#ifndef SHV_CP_TOOLS_H
#define SHV_CP_TOOLS_H
/*! @file
 * The utilities for both unpack and pack.
 */

#include <shv/cp_unpack.h>
#include <shv/cp_pack.h>

/*! Copy next item from unpacker to packer.
 *
 * This copies the whole item. That means the whole list or map as well as whole
 * string or blob. This includes multiple calls to the @ref cp_unpack.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 *   one.
 * @param pack Pack handle.
 * @returns Signals if copy of item was successful. The reason for the failure
 *   is store in `item` or is pack failure.
 */
bool cp_repack(cp_unpack_t unpack, struct cpitem *item, cp_pack_t pack)
	__attribute__((nonnull));

#endif
