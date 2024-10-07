/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_DEVICE_H
#define SHV_RPCHANDLER_DEVICE_H
/*! @file
 * The device SHV API is the standard for the dedicated devices. This handler
 * implements it with ease.
 */

#include <shv/rpcalerts.h>
#include <shv/rpchandler.h>

/*! Object representing SHV RPC Device Alerts Handle.
 *
 * This handle is passed to the callback function `alerts` of @ref
 * rpchandler_device_new.
 */
typedef struct rpchandler_device_alerts *rpchandler_device_alerts_t;

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
 * @param name Name of the device. The string must stay valid for the duration
 *   of the object existence as it is not copied.
 * @param version Version of the device. The string must stay valid for the
 *   duration of the object existence as it is not copied.
 * @param serial_number Serial number device has assigned. It can be `NULL` if
 *   device has no serial number. The string must stay valid for the duration of
 *   the object existence as it is not copied.
 * @param alerts Optional callback that reads device's alerts and packs them.
 *   The implementation should only read alerts and pack them with provided
 *   @ref rpchandler_device_alert API, the initial list around alerts is
 *   already packed by the device handler.
 * @param reset Optional callback that should cause the device reset. You can
 *   pass `NULL` if this is not supported.
 * @returns A new RPC Device Handler object.
 */
rpchandler_device_t rpchandler_device_new(const char *name, const char *version,
	const char *serial_number, void (*alerts)(rpchandler_device_alerts_t),
	void (*reset)(void)) __attribute__((malloc, nonnull(1, 2)));

/*! Free all resources occupied by @ref rpchandler_device_t object.
 *
 * This is destructor for the object created by @ref rpchandler_device_new.
 *
 * @param rpchandler_device RPC Device Handler object.
 */
void rpchandler_device_destroy(rpchandler_device_t rpchandler_device);

/*! Get the RPC Handler stage for this Device Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param rpchandler_device RPC Device Handler object.
 * @returns Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_device_stage(
	rpchandler_device_t rpchandler_device) __attribute__((nonnull));

/*! Get the RPC Handler stage for this Device Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param rpchandler_device RPC Device Handler object.
 * @param handler RPC Handler object.
 */
void rpchandler_device_signal_alerts(rpchandler_device_t rpchandler_device,
	rpchandler_t handler) __attribute__((nonnull));

/*! Adds one device's alert to the packing scheme.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param ctx RPC Handler Device Alerts object.
 * @param alert Pointer to @ref rpcalerts structure.
 * @returns True on success, false otherwise.
 */
bool rpchandler_device_alert(rpchandler_device_alerts_t ctx,
	struct rpcalerts *alert) __attribute__((nonnull));

#endif
