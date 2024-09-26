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
	/*! Called when @ref rpcclient_reset is called and connection is not
	 * established. It can be `NULL` and in such case reconnection is not
	 * possible. This function can set read and write file descriptors to `fd`
	 * parameter or keep or set it to `-1` to signal connection failure. The
	 * returned value signals if reset message should be sent unconditionally
	 * (`true` should be returned if transport can't notify other side about
	 * connection in any other way).
	 */
	bool (*connect)(void *cookie, int fd[2]);
	/*! Called when @ref rpcclient_disconnect or @ref rpcclient_destroy is
	 * called. The argument `destroy` signals if @ref
	 * rpcclient_stream_funcs.connect won't be called again and thus all
	 * reasources should be freed. Be aware that client might be disconnected
	 * before it is destryed and thus make sure that `fd` items are not `-1`.
	 *
	 * The default operation if `NULL` is to simply close file descriptors.
	 */
	void (*disconnect)(void *cookie, int fd[2], bool destroy);
	/*! Called for write file descriptor when message is written. Some protocols
	 * such as TCP would wait for additional data that might not come so instead
	 * this function is called to force immediate send out. The write socket is
	 * provided in `fd`. It can be `NULL` if this is not required.
	 */
	void (*flush)(void *cookie, int fd);
	/*! Called as propagation of @ref rpcclient_peername. The provided `fd` are
	 * read and write file descriptors provided for convenience.
	 */
	size_t (*peername)(void *cookie, int fd[2], char *buf, size_t size);
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

#endif
