/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_DEVICE_H
#define SHV_RPCHANDLER_DEVICE_H
#include <shv/rpcalerts.h>
#include <shv/rpchandler.h>

/**
 * The device SHV API is the standard for the dedicated devices. This handler
 * implements it with ease.
 */

/** Object representing SHV RPC Device Alerts Handle.
 *
 * This handle is passed to the callback function
 * :var:`rpchandler_device_conf.alerts`. It is expected to be used only in that
 * callback by calling :func:`rpchandler_device_alert`.
 */
typedef struct rpchandler_device_alerts *rpchandler_device_alerts_t;

/** Structure used as a configuration of device handler. */
struct rpchandler_device_conf {
	/** Name of the device. The string must stay valid for the duration
	 * of the object existence as it is not copied.
	 */
	const char *name;
	/** Version of the device. The string must stay valid for the
	 * duration of the object existence as it is not copied.
	 */
	const char *version;
	/** Serial number device has assigned. It can be ``NULL``
	 * if device has no serial number. The string must stay valid for the
	 * duration of the object existence as it is not copied.
	 */
	const char *serial_number;
	/** Optional callback that reads device's alerts and packs them.
	 * The implementation should only read alerts and pack them with provided
	 * :c:func:`rpchandler_device_alert` API, the initial list around alerts
	 * is already packed by the device handler.
	 */
	void (*alerts)(rpchandler_device_alerts_t);
	/** Optional callback that should cause the device reset. You
	 * can pass ``NULL`` if this is not supported.
	 */
	void (*reset)(void);
};

/** Get the RPC Handler stage for this Device Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * :param conf: Pointer to :c:struct:`rpchandler_device_conf`. The structure
 *   must stay valid for the duration of the stage existence as it is not
 *   copied. :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_device_stage(
	const struct rpchandler_device_conf *conf);

/** Get the RPC Handler stage for this Device Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * :param conf: RPC Device Handler configuration
 * :param handler: RPC Handler object.
 */
[[gnu::nonnull]]
void rpchandler_device_signal_alerts(
	const struct rpchandler_device_conf *conf, rpchandler_t handler);

/** Adds one device's alert to the packing scheme.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * :param ctx: RPC Handler Device Alerts object.
 * :param alert: Pointer to :c:struct:`rpcalerts` structure.
 * :return: ``true`` on success, ``false`` otherwise.
 */
[[gnu::nonnull]]
bool rpchandler_device_alert(
	rpchandler_device_alerts_t ctx, struct rpcalerts *alert);

#endif
