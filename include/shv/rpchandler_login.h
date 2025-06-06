/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_LOGIN_H
#define SHV_RPCHANDLER_LOGIN_H
#include <shv/rpchandler.h>
#include <shv/rpclogin.h>

/**
 * Handler for the login to the SHV RPC Broker.
 *
 * This should be the first handler in the sequence of the stages to ensure that
 * before anything else happens the client gets logged-in to the broker.
 *
 * This handler also keeps you connected by pinging the broker if there was not
 * activity for some time.
 */

/** Object representing SHV RPC Login Handle.
 *
 * It provides login functionality and should be the first handler in the
 * sequence to manage all messages until login is successful.
 */
typedef struct rpchandler_login *rpchandler_login_t;

/** Create new RPC Login Handle.
 *
 * :param login: Login parameters that are used to perform the login. It is
 *   used as a reference and thus it must be valid for the duration of this
 *   object existence.
 * :return: A new RPC Login Handler object.
 */
[[gnu::nonnull, gnu::malloc]]
rpchandler_login_t rpchandler_login_new(const struct rpclogin *login);

/** Free all resources occupied by :c:type:`rpchandler_login_t` object.
 *
 * This is destructor for the object created by :c:func:`rpchandler_login_new`.
 *
 * :param rpchandler_login: RPC Login Handler object.
 */
void rpchandler_login_destroy(rpchandler_login_t rpchandler_login);

/** Get the RPC Handler stage for this Login Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * This stage should be the first stage in the sequence to ensure that login is
 * performed before anything else is allowed.
 *
 * :param rpchandler_login: RPC Login Handler object.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_login_stage(rpchandler_login_t rpchandler_login);

/** Query the current login process status.
 *
 * :param rpchandler_login: RPC Login Handler object.
 * :param errnum: Pointer to the variable where error number that Broker
 *   responded with on login is stored. Can be ``NULL`` if you are not
 *   interested.
 * :param errmsg: Pointer to the variable where error string that
 *   Broker responded with on login is stored. Can be ``NULL`` if you are not
 *   interested. This is set only if ``errnum`` is not equal
 *   :c:macro:`RPCERR_NO_ERROR`. Note that this string is kept valid only until
 *   object is valid and handler is not running again. The login error will
 *   signal error to the RPC Handler which should terminate it but it can be
 *   started again and in such case this string will be freed.
 *
 * :return: ``true`` if login was performed and ```false` otherwise. On ``true``
 *   you must investigate ``errnum`` parameter to find out if login was
 *   successful.
 */
[[gnu::nonnull(1)]]
bool rpchandler_login_status(rpchandler_login_t rpchandler_login,
	rpcerrno_t *errnum, const char **errmsg);

/** Wait until login is performed.
 *
 * :param rpchandler_login: RPC Login Handler object.
 * :param errnum: Pointer to the variable where error number that Broker
 *   responded with on login is stored. Can be ``NULL`` if you are not
 *   interested.
 * :param errmsg: Pointer to the variable where error string that Broker
 *   responded with on login is stored. Can be ``NULL`` if you are not
 *   interested. This is set only if ``errnum`` is not equal
 *   :c:macro:`RPCERR_NO_ERROR`. Note that this string is kept valid only until
 *   object is valid and handler is not running again. The login error will
 *   signal error to the RPC Handler which should terminate it but it can be
 *   started again and in such case this string will be freed.
 * :param abstime: The absolute time when wait will result into a timeout. It
 *   can be ``NULL`` if it should never timeout.
 * :return: ``true`` if login was performed and ``false`` if wait timed out or
 *   failed. You can inspect ``errnum`` to identify why.
 */
[[gnu::nonnull(1)]]
bool rpchandler_login_wait(rpchandler_login_t rpchandler_login,
	rpcerrno_t *errnum, const char **errmsg, struct timespec *abstime);

#endif
