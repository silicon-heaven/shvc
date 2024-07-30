/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCURL_H
#define SHV_RPCURL_H
/*! @file
 * RPC URL is universal identifier that is used to specify the way client
 * connects to the server as well as server listen configuration. The URL is a
 * complete set parameters needed to connect client (including the client
 * device) to the SHV network.
 */

#include <shv/rpcclient.h>
#include <shv/rpclogin.h>
#include <shv/rpcserver.h>

/*! Protocol used to connect client to the server or to listen for connection on
 */
enum rpc_protocol {
	/*! TCP/IP with SHV RPC Block link layer protocol. */
	RPC_PROTOCOL_TCP,
	/*! TCP/IP with SHV RPC Serial link layer protocol. */
	RPC_PROTOCOL_TCPS,
	/*! TLS TCP/IP with SHV RPC Bloc link layer protocol. */
	RPC_PROTOCOL_SSL,
	/*! TLS TCP/IP with SHV RPC Serial link layer protocol. */
	RPC_PROTOCOL_SSLS,
	/*! Unix domain name socket with SHV RPC Block link layer protocol. */
	RPC_PROTOCOL_UNIX,
	/*! Unix domain name socket with SHV RPC Serial link layer protocol. */
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
		/*! SHV RPC URL SSL specific options. */
		struct rpcurl_ssl {
			/*! Path to the file with CA certificates. */
			const char *ca;
			/*! Path to the file with certificate. */
			const char *cert;
			/*! Path to the file with secret part of the `cert`. */
			const char *key;
			/*! Path to the file with certification revocation list. */
			const char *crl;
			/*! Controls if server certificate is validated by client. */
			bool verify;
		}
		/*! Use this to access SSL specific options for @ref RPC_PROTOCOL_SSL
		 * and @ref RPC_PROTOCOL_SSLS.
		 */
		ssl;
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


/*! Create RPC client based on the provided URL.
 *
 * @param url RPC URL with connection info. The URL location string must stay
 *   valid and unchanged during the client's existence because it is used
 *   directly by created client object.
 * @returns SHV RPC client handle.
 */
rpcclient_t rpcurl_new_client(const struct rpcurl *url);

/*! Establish a new connection based on the provided URL.
 *
 * This is convenient combination of @ref rpcurl_new_client and @ref
 * rpcclient_reset.
 *
 * @param url RPC URL with connection info.
 * @returns SHV RPC client handle or `NULL` in case connection failed; you can
 *   investigate the `errno` to identify why that might have been the case.
 */
static inline rpcclient_t rpcurl_connect_client(const struct rpcurl *url) {
	rpcclient_t res = rpcurl_new_client(url);
	if (rpcclient_reset(res))
		return res;
	rpcclient_destroy(res);
	return NULL;
}

/*! Create RPC server based on the provided URL.
 *
 * @param url RPC URL with listen info. The URL location string must stay
 *   valid and unchanged during the server's existence because it is used
 *   directly by created server object.
 * @returns SHV RPC server handle.
 */
rpcserver_t rpcurl_new_server(const struct rpcurl *url);

#endif
