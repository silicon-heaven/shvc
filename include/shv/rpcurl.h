/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCURL_H
#define SHV_RPCURL_H
#include <shv/rpcclient.h>
#include <shv/rpclogin.h>
#include <shv/rpcserver.h>

/**
 * RPC URL is universal identifier that is used to specify the way client
 * connects to the server as well as server listen configuration. The URL is a
 * complete set parameters needed to connect client (including the client
 * device) to the SHV network.
 */

/** Default TPC/IP port used for RPC protocol TCP. */
#define RPCURL_TCP_PORT (3755)
/** Default TPC/IP port used for RPC protocol TCPS. */
#define RPCURL_TCPS_PORT (3765)
/** Default TPC/IP port used for RPC protocol TCP. */
#define RPCURL_SSL_PORT (3756)
/** Default TPC/IP port used for RPC protocol TCPS. */
#define RPCURL_SSLS_PORT (3766)

/** Protocol used to connect client to the server or to listen for connection on
 */
enum rpc_protocol {
	/** TCP/IP with SHV RPC Block link layer protocol. */
	RPC_PROTOCOL_TCP,
	/** TCP/IP with SHV RPC Serial link layer protocol. */
	RPC_PROTOCOL_TCPS,
	/** TLS TCP/IP with SHV RPC Bloc link layer protocol. */
	RPC_PROTOCOL_SSL,
	/** TLS TCP/IP with SHV RPC Serial link layer protocol. */
	RPC_PROTOCOL_SSLS,
	/** Unix domain name socket with SHV RPC Block link layer protocol. */
	RPC_PROTOCOL_UNIX,
	/** Unix domain name socket with SHV RPC Serial link layer protocol. */
	RPC_PROTOCOL_UNIXS,
	/** Serial port with SHV RPC Stream link layer used to protocol messages. */
	RPC_PROTOCOL_TTY,
};


/** SHV RPC URL representation */
struct rpcurl {
	/** Protocol used to connect to the server */
	enum rpc_protocol protocol;
	/** Location where client should connect to or server listen on */
	const char *location;
	/** Port number used for TCP and UDP connections */
	int port;
	/** Login options */
	struct rpclogin login;

	union {
		/** Additional options available only for the
		 * :c:enumerator:`RPC_PROTOCOL_TTY`.
		 */
		struct rpcurl_tty {
			/** Baudrate to be used when communication over TTY. */
			unsigned baudrate;
		}
		/** Use this to access additional options for
		 * :c:enumerator:`RPC_PROTOCOL_TTY`.
		 */
		tty;
		/** SHV RPC URL SSL specific options. */
		struct rpcurl_ssl {
			/** Path to the file with CA certificates. */
			const char *ca;
			/** Path to the file with certificate. */
			const char *cert;
			/** Path to the file with secret part of the `cert`. */
			const char *key;
			/** Path to the file with certification revocation list. */
			const char *crl;
			/** Controls if server certificate is not validated by client. */
			bool noverify;
		}
		/** Use this to access SSL specific options for
		 * :c:enumerator:`RPC_PROTOCOL_SSL` and
		 * :c:enumerator:`RPC_PROTOCOL_SSLS`.
		 */
		ssl;
	};
};



/** Parse string URL.
 *
 * This function uses obstack to allocate parsed URL. This way you can free
 * the whole parsed URL by freeing the returned address.
 *
 * :param url: String representation of URL
 * :param errpos: Pointer to the position of error in the URL string.
 *   ``NULL`` can be passed if you are not interested in the error location.
 * :param obstack: obstack used for allocation.
 * :return: Pointer to the :c:struct:`rpcurl` or ``NULL`` in case of URL parse
 *   error.
 */
[[gnu::nonnull(1, 3)]]
struct rpcurl *rpcurl_parse(
	const char *url, const char **errpos, struct obstack *obstack);

/** Convert RPC URL to string.
 *
 * :param rpcurl: :c:struct:`rpcurl` to be converted to string.
 * :param buf: Pointer to the buffer where string will be stored. You can pass
 *   ``NULL`` alongside with ``size == 0`` and in such case this function only
 *   calculates amount of space required for the string.
 * :param size: Size of the ``buf`` in bytes.
 * :return: Number of bytes written (excluding the terminating null byte). If
 *   buffer is too small then it returns number bytes it would wrote. This can
 *   be used to detect truncated output due to small buffer.
 */
[[gnu::nonnull(1)]]
size_t rpcurl_str(const struct rpcurl *rpcurl, char *buf, size_t size);

/** Create RPC client based on the provided URL.
 *
 * :param url: RPC URL with connection info. The URL location string must stay
 *   valid and unchanged during the client's existence because it is used
 *   directly by created client object.
 * :return: SHV RPC client handle.
 */
[[gnu::nonnull]]
rpcclient_t rpcurl_new_client(const struct rpcurl *url);

/** Establish a new connection based on the provided URL.
 *
 * This is convenient combination of :c:func:`rpcurl_new_client` and
 * :c:func:`rpcclient_reset`.
 *
 * :param url: RPC URL with connection info.
 * :return: SHV RPC client handle or ``NULL`` in case connection failed; you can
 *   investigate the ``errno`` to identify why that might have been the case.
 */
[[gnu::nonnull]]
static inline rpcclient_t rpcurl_connect_client(const struct rpcurl *url) {
	rpcclient_t res = rpcurl_new_client(url);
	if (!res)
		return NULL;
	if (rpcclient_reset(res))
		return res;
	rpcclient_destroy(res);
	return NULL;
}

/** Create RPC server based on the provided URL.
 *
 * :param url: RPC URL with listen info. The URL location string must stay
 *   valid and unchanged during the server's existence because it is used
 *   directly by created server object.
 * :return: SHV RPC server handle.
 */
[[gnu::nonnull]]
rpcserver_t rpcurl_new_server(const struct rpcurl *url);

#endif
