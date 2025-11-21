/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_APP_H
#define SHV_RPCHANDLER_APP_H
#include <shv/rpchandler.h>

/**
 * The Application SHV API is requires by standard to be present for devices and
 * optional for other clients. This handler implements it for easy inclusion.
 */

/** Structure used as a configuration of app handler. */
struct rpchandler_app_conf {
	/** Name of the application or ``NULL`` to report ``shvc``. The
	 * string must stay valid for the duration of the object existence as it
	 * is not copied.
	 */
	const char *name;
	/**	Version of the application or ``NULL`` to report SHVC
	 * version. The string must stay valid for the duration of the object
	 * existence as it is not copied. You should use both ``name`` and
	 * ``version`` or none to prevent confusion
	 */
	const char *version;
	/** ``True`` if method ``.app:date`` is not present, ``False`` otherwise */
	bool disable_date;
};

/** Get the RPC Handler stage for this Application Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * This stage can be reused in multiple handlers.
 *
 * :param conf: Pointer to :c:struct:`rpchandler_app_conf`. The structure must
 *  stay valid for the duration of the stage existence as it is not copied.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_app_stage(
	const struct rpchandler_app_conf *conf);

#endif
