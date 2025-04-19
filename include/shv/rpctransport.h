/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCTRANSPORT_H
#define SHV_RPCTRANSPORT_H
#include <shv/rpcclient_stream.h>
#include <shv/rpcserver.h>

/**
 * SHV RPC clients and servers for supported transport layers.
 */


/** Create a new TCP/IP client.
 *
 * :param location: Location of the TCP server. The string must be valid all the
 *   time during the client's existence.
 * :param port: Port of the TCP server (commonly 3755).
 * :param proto: Stream protocol to be used (commonly
 *   :c:enumerator:`RPCSTREAM_P_BLOCK`) :return: SHV RPC client handle.
 */
[[gnu::nonnull, gnu::malloc]]
rpcclient_t rpcclient_tcp_new(
	const char *location, int port, enum rpcstream_proto proto);

/** Create a new TCP/IP server.
 *
 * :param location: Location to which TCP server binds itself.
 * :param port: Port server will listen on.
 * :param proto: Stream protocol to be used (commonly
 *   :c:enumerator:`RPCSTREAM_P_BLOCK`) :return: SHV RPC server handle.
 */
[[gnu::nonnull, gnu::malloc]]
rpcserver_t rpcserver_tcp_new(
	const char *location, int port, enum rpcstream_proto proto);

/** Create a new Unix client.
 *
 * :param location: Location of the Unix socket. The string must be valid all
 *   the time during the client's existence.
 * :param proto: Stream protocol to be used (commonly
 *   :c:enumerator:`RPCSTREAM_P_SERIAL`) :return: SHV RPC client handle.
 */
[[gnu::nonnull, gnu::malloc]]
rpcclient_t rpcclient_unix_new(const char *location, enum rpcstream_proto proto);

/** Create a new Unix server.
 *
 * :return location: Location of Unix socket server binds itself to.
 * :return proto: Stream protocol to be used (commonly
 *   :c:enumerator`RPCSTREAM_P_SERIAL`) :return: SHV RPC server handle.
 */
[[gnu::nonnull, gnu::malloc]]
rpcserver_t rpcserver_unix_new(const char *location, enum rpcstream_proto proto);

/** Create a new TTY client.
 *
 * :param location: Location of the TTY device. The string must be valid all
 *   the time during the client's existence.
 * :param baudrate: Communication speed to be set on the TTY device.
 * :param proto: Stream protocol to be used (commonly
 *   :c:enumerator:`RPCSTREAM_P_SERIAL_CRC`) :result: SHV RPC client handle.
 */
[[gnu::nonnull]]
rpcclient_t rpcclient_tty_new(
	const char *location, unsigned baudrate, enum rpcstream_proto proto);

/** Create a new TTY server.
 *
 * Note that this is not a real server. TTY supports only one client (there is
 * no connection multiplexing). Instead this provides ability to wait for the
 * specified TTY to appear and reappear.
 *
 * :param location: Location of TTY server should wait for.
 * :param baudrate: Communication speed to be set on the TTY device.
 * :param proto: Stream protocol to be used (commonly
 *   :c:enumerator:`RPCSTREAM_P_SERIAL_CRC`) :return: SHV RPC server handle.
 */
[[gnu::nonnull, gnu::malloc]]
rpcserver_t rpcserver_tty_new(
	const char *location, unsigned baudrate, enum rpcstream_proto proto);

/** Create a new client on top of Unix pipes.
 *
 * This opens two pipe pairs and provides one end to you as pipes and other end
 * as RPC client.
 *
 * This is handy for the inter-process communication when forking the
 * subprocess.
 *
 * :param pipes: Array where read and write pipe file descriptors will be placed
 *   to. The first one is the read and the second one is the write pipe.
 * :param proto: Stream protocol to be used (commonly
 *   :c:enumerator:`RPCSTREAM_P_SERIAL`) :return: SHV RPC client handle.
 */
[[gnu::nonnull]]
rpcclient_t rpcclient_pipe_new(int pipes[2], enum rpcstream_proto proto);

#endif
