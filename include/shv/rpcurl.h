#ifndef SHV_RPCURL_H
#define SHV_RPCURL_H

/*! Protocol used to connect client to the server or to listen for connection on
 */
enum rpc_protocol {
	/*! TCP/IP protocol with SHV RPC Stream link layer used to transport
	 * messages.
	 */
	RPC_PROTOCOL_TCP,
	/*! UDP/IP protocol with SHV RPC Datagram link layer used to transport
	 * messages.
	 */
	RPC_PROTOCOL_UDP,
	/*! Unix domain name socket with SHV RPC Stream link layer used to transport
	 * messages.
	 */
	RPC_PROTOCOL_LOCAL_SOCKET,
	/*! Serial port with SHV RPC Stream link layer used to transport messages. */
	RPC_PROTOCOL_SERIAL_PORT,
};

/*! The format of the password passed to the login process. */
enum rpclogin_type {
	/*! The password is in plain text */
	RPC_LOGIN_PLAIN,
	/*! Password is hashed with SHA1 password */
	RPC_LOGIN_SHA1,
};

struct rpclogin_options {
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

/*! SHV RPC URL representation */
struct rpcurl {
	/*! Protocol used to connect to the server */
	enum rpc_protocol protocol;
	/*! Location where client should connect to or server listen on */
	const char *location;
	/*! Port number used for TCP and UDP connections */
	int port;
	/*! Login options */
	struct rpclogin_options login;
};


/*! Parse string URL.
 *
 * @param url: String representation of URL
 * @param error_pos: Pointer to the position of error in the URL string.
 *   `NULL` can be passed if you are not interested in the error location.
 * @returns `struct rpcurl` allocated using `malloc`. or `NULL` in case of URL
 *   parse error. Do not free returned memory using `free`, you need to use
 *   `rpcurl_free` instead.
 */
struct rpcurl *rpcurl_parse(const char *url, const char **error_pos)
	__attribute__((nonnull(1)));

/*! Free RPC URL representation previously parsed using by `rpcurl_parse`.
 *
 * @param rpc_url: pointer returned by `rpcurl_parse` or `NULL`.
 */
void rpcurl_free(struct rpcurl *rpc_url);

/*! Convert RPC URL to the string representation.
 *
 * @param rpc_url: Pointer to the RPC URL to be converted to the string.
 * @returns `malloc` allocated string with URL.
 */
char *rpcurl_str(struct rpcurl *rpc_url) __attribute__((nonnull));

#endif
