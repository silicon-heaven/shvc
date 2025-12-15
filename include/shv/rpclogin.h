/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCLOGIN_H
#define SHV_RPCLOGIN_H
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>

/**
 * RPC Login provides definition and common login utilities.
 */

/** Maximum number of characters SHV standard defines for nonce provided by
 * SHV RPC method ``hello``.
 */
#define SHV_NONCE_MAXLEN (32)

/** Default timeout for :c:var:`rpclogin.idle_timeout`. */
#define SHV_IDLE_TIMEOUT_DEFAULT (180)


/** The format of the password passed to the login process. */
enum rpclogin_type {
	/** The password is in plain text */
	RPC_LOGIN_PLAIN,
	/** Password is hashed with SHA1 password */
	RPC_LOGIN_SHA1,
};

/** Options used for login to the RPC Broker.  */
struct rpclogin {
	/** User name used to login client connection to the server */
	const char *username;
	/** Password used to login client connection to the server */
	const char *password;
	/** The format of the password */
	enum rpclogin_type login_type;
	/** Timeout to be specified on the server in number of seconds before
	 * automatic disconnect. You can use ``0`` to not specify this option and
	 * thus use default value of the server.
	 *
	 * This is login option ``idleWatchDogTimeOut``.
	 */
	unsigned idle_timeout;
	/** Device ID sent as part of login information to the server.
	 *
	 * This is login option ``device.deviceId``.
	 */
	const char *device_id;
	/** Device's requested mount point on server.
	 *
	 * This is login option ``device.mountPoint``.
	 */
	const char *device_mountpoint;
};


/** Pack login parameter.
 *
 * :param pack: The generic packer where it is about to be packed.
 * :param login: The login to be packed.
 * :param nonce: The nonce string provided by SHV RPC ``hello`` method.
 * :param trusted: If transport layer is trusted to not disclouse the password.
 *   This controls elevation of :c:enumerator:`RPC_LOGIN_PLAIN` to
 *   :c:enumerator:`RPC_LOGIN_SHA1` to increase security on unencrypted
 *   transport layers.
 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
 */
[[gnu::nonnull]]
bool rpclogin_pack(cp_pack_t pack, const struct rpclogin *login,
	const char *nonce, bool trusted);

/** Unpack login parameter.
 *
 * This stores the whole item in the obstack. To free everything (including what
 * was allocated in the obstack after call to this function) you can free with
 * pointer returned from this function.
 *
 * :param unpack: The generic unpacker to be used to unpack items.
 * :param item: The item instance that was used to unpack the previous item.
 * :param obstack: Pointer to the obstack instance to be used to allocate
 *   storage for the unpacked items.
 * :return: Pointer to the unpacked login  parameter or ``NULL`` in case of an
 * 	 unpack error. You can investigate ``item`` to identify the failure cause.
 */
[[gnu::nonnull]]
struct rpclogin *rpclogin_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);

/** Validate password of the login against provided reference.
 *
 * This performs login validation and is expected to be used after
 * :c:func:`rpclogin_unpack`.
 *
 * :param login: The login that should be verified.
 * :param password: The authoritative password.
 * :param nonce: Nonce used for SHA1 login.
 * :param type: The format of the `password`.
 * :return: ``true`` in case login matches the authoritation password and
 *   ``false`` otherwise.
 */
[[gnu::nonnull(1, 2)]]
bool rpclogin_validate_password(const struct rpclogin *login,
	const char *password, const char *nonce, enum rpclogin_type type);

#endif
