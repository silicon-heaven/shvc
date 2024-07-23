/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_STREAM_H
#define SHV_RPCCLIENT_STREAM_H
/*! @file
 * Common implementation of the RPC client for the stream transport layers.
 */

#include <shv/rpcclient.h>

/*! RPC Client Stream transport layer protocol selection. */
enum rpcstream_proto {
	/*! This is the Block protocol where messages are prefixed with their size.
	 */
	RPCSTREAM_P_BLOCK,
	/*! This is the Serial protocol that specific bytes to mark start and the
	 * end of the message.
	 */
	RPCSTREAM_P_SERIAL,
	/*! This is variant of @ref RPCSTREAM_P_SERIAL that tracks and validates
	 * CRC32 of the transported messages to detect corrupted data.
	 */
	RPCSTREAM_P_SERIAL_CRC,
};

/*! The set of functions that are used to delegate some of the functionality of
 * the RPC Client.
 */
struct rpcclient_stream_funcs {
	/*! Called when `rpcclient_reset` is called and connection is not
	 * established. It can be `NULL` and in such case reconnection is not
	 * possible. This function must set read and write to `fd` parameter.
	 */
	void (*connect)(void *cookie, int fd[2]);
	/*! Called for write file descriptor when message is written. Some protocols
	 * such as TCP would wait for additional data that might not come so instead
	 * this function is called to force immediate send out. The write socket is
	 * provided in `fd`. It can be `NULL` if this is not required.
	 */
	void (*flush)(void *cookie, int fd);
	/*! Called as part of the client object destruction and should be used to
	 * free the cookie.
	 */
	void (*free)(void *cookie);
};


/*! Create new stream based RPC Client object.
 *
 * This is expected to work with sockets and pipes. The socket creation and few
 * other tasks are delegated but rest is managed internally based on the POSIX
 * socket functionality.
 *
 * @param sclient Pointer to the set of function pointers that are called in
 *   various situation during the client's lifetime.
 * @param sclient_cookie Pointer passed to the `sclient` functions.
 * @param proto The chosen stream protocol.
 * @param rfd Read socket. You should use `-1` if connection is not yet
 *   established (in such case @ref rpcclient_stream_funcs.connect will be
 *   used). @ref rpcclient_destroy closes this socket only if @ref
 *   rpcclient_stream_funcs.connect is not `NULL`.
 * @param wfd Write socket. It can be same as read socket (for example for TCP)
 *   and even if it is not the same the same behaviour applyes.
 */
rpcclient_t rpcclient_stream_new(const struct rpcclient_stream_funcs *sclient,
	void *sclient_cookie, enum rpcstream_proto proto, int rfd, int wfd)
	__attribute__((nonnull(1)));


/*! Create a new TCP client.
 *
 * @param location Location of the TCP server. The string must be valid all the
 *   time during the client's existence.
 * @param port Port of the TCP server (commonly 3755).
 * @param proto Stream protocol to be used (commonly @ref RPCSTREAM_P_BLOCK)
 * @returns SHV RPC client handle.
 */
rpcclient_t rpcclient_tcp_new(const char *location, int port,
	enum rpcstream_proto proto) __attribute__((nonnull, malloc));

/*! Create a new Unix client.
 *
 * @param location Location of the Unix socket. The string must be valid all
 * the time during the client's existence.
 * @param proto Stream protocol to be used (commonly @ref RPCSTREAM_P_BLOCK)
 * @returns SHV RPC client handle.
 */
rpcclient_t rpcclient_unix_new(const char *location, enum rpcstream_proto proto)
	__attribute__((nonnull, malloc));

/*! Create a new TTY client.
 *
 * @param location Location of the TTY device. The string must be valid all
 * the time during the client's existence.
 * @param baudrate Communication speed to be set on the TTY device.
 * @param proto Stream protocol to be used (commonly @ref
 * RPCSTREAM_P_SERIAL_CRC)
 * @returns SHV RPC client handle.
 */
rpcclient_t rpcclient_tty_new(const char *location, unsigned baudrate,
	enum rpcstream_proto proto) __attribute__((nonnull));

/*! Create a new client on top of Unix pipes.
 *
 * This opens two pipe pairs and provides one end to you as pipes and other end
 * as RPC client.
 *
 * This is handy for the inter-process communication when forking the
 * subprocess.
 *
 * @param pipes Array where read and write pipe file descriptors will be placed
 *   to. The first one is the read and the second one is the write pipe.
 * @param proto Stream protocol to be used (commonly @ref RPCSTREAM_P_SERIAL)
 * @returns SHV RPC client handle.
 */
rpcclient_t rpcclient_pipe_new(int pipes[2], enum rpcstream_proto proto)
	__attribute__((nonnull));

#endif
