/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCURL_H
#define SHV_RPCURL_H
/*! @file
 * RPC URL is universal identifier that is used to specify the way client
 * connects to the server as well as server listen configuration. The URL is a
 * complete set parameters needed to connect client (including the client
 * device) to the SHV network.
 */

#include <shv/rpclogin.h>

/*! Protocol used to connect client to the server or to listen for connection on
 */
enum rpc_protocol {
	/*! TCP/IP protocol with SHV RPC Stream link layer used to protocol
	 * messages.
	 */
	RPC_PROTOCOL_TCP,
	/*! TCP/IP protocol with SHV RPC Serial link layer used to protocol
	 * messages.
	 */
	RPC_PROTOCOL_TCPS,
	/*! Unix domain name socket with SHV RPC Stream link layer used to protocol
	 * messages.
	 */
	RPC_PROTOCOL_UNIX,
	/*! Unix domain name socket with SHV RPC Serial link layer used to protocol
	 * messages.
	 */
	RPC_PROTOCOL_UNIXS,
	/*! Serial port with SHV RPC Stream link layer used to protocol messages. */
	RPC_PROTOCOL_TTY,
};

/*! SHV RPC URL representation */
struct rpcurl {
	/*! Protocol used to connect to the server */
	enum rpc_protocol protocol;
	/*! Location where client should connect to or server listen on */
	const char *location;
	/*! Port number used for TCP and UDP connections */
	int port;
	/*! Login options */
	struct rpclogin login;

	// @cond
	union {
		// @endcond
		/*! Additional options available only for the @ref RPC_PROTOCOL_TTY. */
		struct rpcurl_tty {
			/*! Baudrate to be used when communication over TTY. */
			unsigned baudrate;
		}
		/*! Use this to access additional options for @ref RPC_PROTOCOL_TTY. */
		tty;
		// @cond
	};
	// @endcond
};


/*! Parse string URL.
 *
 * @param url String representation of URL
 * @param error_pos Pointer to the position of error in the URL string.
 *   `NULL` can be passed if you are not interested in the error location.
 * @returns `struct rpcurl` allocated using `malloc`. or `NULL` in case of URL
 *   parse error. Do not free returned memory using `free`, you need to use
 *   `rpcurl_free` instead.
 */
struct rpcurl *rpcurl_parse(const char *url, const char **error_pos)
	__attribute__((nonnull(1)));

/*! Free RPC URL representation previously parsed using by `rpcurl_parse`.
 *
 * @param rpc_url pointer returned by `rpcurl_parse` or `NULL`.
 */
void rpcurl_free(struct rpcurl *rpc_url);

/*! Convert RPC URL to the string representation.
 *
 * @param rpc_url Pointer to the RPC URL to be converted to the string.
 * @returns `malloc` allocated string with URL.
 */
char *rpcurl_str(struct rpcurl *rpc_url) __attribute__((nonnull));

#endif
