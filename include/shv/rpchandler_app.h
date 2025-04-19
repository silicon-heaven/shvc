/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_APP_H
#define SHV_RPCHANDLER_APP_H
#include <shv/rpchandler.h>

/**
 * The Application SHV API is requires by standard to be present for devices and
 * optional for other clients. This handler implements it for easy inclusion.
 */

/** Object representing SHV RPC Application Handle.
 *
 * It provides application API functionality as defined by SHV standard. That
 * include methods:
 * * ``.app:shvVersionMajor``
 * * ``.app:shvVersionMinor``
 * * ``.app:name``
 * * ``.app:version``
 * * ``.app:ping``
 * * ``.app:date``
 */
typedef struct rpchandler_app *rpchandler_app_t;

/** Create new RPC Application Handle.
 *
 * :param name: Name of the application or ``NULL`` to report ``shvc``. The
 *   string must stay valid for the duration of the object existence as it is
 *   not copied.
 * :param version: Version of the application or ``NULL`` to report SHVC
 *   version. The must stay valid for the duration of the object existence as it
 *   is not copied. You should use both ``name`	 and ``version`` or none to
 *   prevent confusion.
 * :return: A new RPC Application Handler object.
 */
[[gnu::nonnull]]
rpchandler_app_t rpchandler_app_new(const char *name, const char *version);

/** Free all resources occupied by :c:type:`rpchandler_app_t` object.
 *
 * This is destructor for the object created by :c:func:`rpchandler_app_new`.
 *
 * :param rpchandler_app: RPC Application Handler object.
 */
void rpchandler_app_destroy(rpchandler_app_t rpchandler_app);

/** Get the RPC Handler stage for this Application Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * This stage can be reused in multiple handlers.
 *
 * :param rpchandler_app: RPC Application Handler object.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_app_stage(rpchandler_app_t rpchandler_app);


#endif
