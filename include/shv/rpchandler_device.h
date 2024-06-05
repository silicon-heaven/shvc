/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_DEVICE_H
#define SHV_RPCHANDLER_DEVICE_H
/*! @file
 * The device SHV API is the standard for the dedicated devices. This handler
 * implements it with ease.
 */

#include <shv/rpchandler.h>

/*! Object representing SHV RPC Application Handle.
 *
 * It provides device API functionality as defined by SHV standard. That
 * include methods:
 * * `.device:name`
 * * `.device:version`
 * * `.device:serialNumber`
 * * `.device:uptime`
 * * `.device:reset`
 */
typedef struct rpchandler_device *rpchandler_device_t;

/*! Create new RPC Device Handle.
 *
 * @param name: Name of the device. The string must stay valid for the duration
 *   of the object existence as it is not copied.
 * @param version: Version of the device. The string must stay valid for the
 *   duration of the object existence as it is not copied.
 * @param serial_number: Serial number device has assigned. It can be `NULL` if
 *   device has no serial number. The string must stay valid for the duration of
 *   the object existence as it is not copied.
 * @param reset: Optional callback that should cause the device reset. You can
 *   pass `NULL` if this is not supported.
 * @returns A new RPC Device Handler object.
 */
rpchandler_device_t rpchandler_device_new(const char *name, const char *version,
	const char *serial_number, void (*reset)(void))
	__attribute__((malloc, nonnull(1, 2)));

/*! Free all resources occupied by @ref rpchandler_device_t object.
 *
 * This is destructor for the object created by @ref rpchandler_device_new.
 *
 * @param rpchandler_device: RPC Device Handler object.
 */
void rpchandler_device_destroy(rpchandler_device_t rpchandler_device);

/*! Get the RPC Handler stage for this Device Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * This stage can be reused in multiple handlers.
 *
 * @param rpchandler_device: RPC Device Handler object.
 * @returns Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_device_stage(
	rpchandler_device_t rpchandler_device) __attribute__((nonnull));


#endif
