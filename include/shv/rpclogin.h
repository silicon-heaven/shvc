/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCLOGIN_H
#define SHV_RPCLOGIN_H
/*! @file
 * RPC Login provides definition and common login utilities.
 */

#include <shv/cp_pack.h>

/*! Maximum number of characters SHV standard defines for nonce provided by
 * SHV RPC method `hello`.
 */
#define SHV_NONCE_MAXLEN (32)


/*! The format of the password passed to the login process. */
enum rpclogin_type {
	/*! The password is in plain text */
	RPC_LOGIN_PLAIN,
	/*! Password is hashed with SHA1 password */
	RPC_LOGIN_SHA1,
};

/*! Options used for login to the RPC Broker.  */
struct rpclogin {
	/*! User name used to login client connection to the server */
	const char *username;
	/*! Password used to login client connection to the server */
	const char *password;
	/*! The format of the password */
	enum rpclogin_type login_type;
	/*! Device ID sent as part of login information to the server */
	const char *device_id;
	/*! Device's requested mount point on server */
	const char *device_mountpoint;
};


/*! Pack login parameter.
 *
 * @param pack The generic packer where it is about to be packed.
 * @param login The login to be packed.
 * @param nonce The nonce string provided by SHV RPC `hello` method.
 * @param trusted If transport layer is trusted to not disclouse the password.
 *   This controls elevation of @ref RPC_LOGIN_PLAIN to @ref RPC_LOGIN_SHA1 to
 *   increase security on unencrypted transport layers.
 * @returns `fasle` if packing encounters failure and `false` otherwise.
 */
bool rpclogin_pack(cp_pack_t pack, const struct rpclogin *login,
	const char *nonce, bool trusted) __attribute__((nonnull));

#endif
