/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_H
#define SHV_RPCCLIENT_H
/*! @file
 * Handle managing a single connection for SHV RPC.
 */

#include <stdbool.h>
#include <time.h>
#include <shv/rpcurl.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <shv/rpclogger.h>
#include <shv/rpcprotocol.h>
#include "../../libshvrpc/rpcprotocol_common.h"

/*! Number of seconds that is default idle time before brokers disconnect
 * clients for inactivity.
 */
#define RPC_DEFAULT_IDLE_TIME 180

/*! Operations performed by control function for RPC Client.
 *
 * To reduce the size of the @ref rpcclient  structure we use only one function
 * pointer and pass operation as argument. We define macros to hide this but
 * also because control function must be called with pointer to the RPC Client
 * and thus using macros makes is safer and more easy to read.
 */
enum rpcclient_ctlop {
	/*! @ref rpcclient_destroy */
	RPCC_CTRLOP_DESTROY,
	/*! @ref rpcclient_errno and @ref rpcclient_connected */
	RPCC_CTRLOP_ERRNO,
	/*! @ref rpcclient_nextmsg */
	RPCC_CTRLOP_NEXTMSG,
	/*! @ref rpcclient_validmsg */
	RPCC_CTRLOP_VALIDMSG,
	/*! @ref rpcclient_sendmsg */
	RPCC_CTRLOP_SENDMSG,
	/*! @ref rpcclient_dropmsg */
	RPCC_CTRLOP_DROPMSG,
	/*! @ref rpcclient_pollfd */
	RPCC_CTRLOP_POLLFD,
};

/*! Public definition of RPC Client object.
 *
 * It provides abstraction on top of multiple different protocols providing
 * transfer of SHV RPC messages.
 */
struct rpcclient {
	/*! Control function. Do not use directly. */
	int (*ctl)(struct rpcclient *, enum rpcclient_ctlop);

	/*! Transport layer used for communication. Do not use directly. */
	struct rpcprotocol_interface *protocol_interface;

	int fd;

	/*! Unpack function. Please use @ref rpcclient_unpack instead. */
	cp_unpack_func_t unpack;

	/*! Pack function. Please use @ref rpcclient_pack instead. */
	cp_pack_func_t pack;

	/*! Loggers used to log messages received by this client. */
	rpclogger_t logger_in;
	/*! Loggers used to log message sent by this client. */
	rpclogger_t logger_out;
};

/*! Handle used to manage SHV RPC client. */
typedef struct rpcclient *rpcclient_t;


/*! Establish a new connection based on the provided URL.
 *
 * This only estableshes the connection. You might need to perform login if you
 * are connecting to the SHV Broker.
 *
 * @param url: RPC URL with connection info.
 * @returns SHV RPC handle or `NULL` in case connection failed. Based on the
 */
rpcclient_t rpcclient_connect(const struct rpcurl *url);


/*! Create new RPC Client with Stream protocol_interface layer.
 *
 * The protocol_interface layer description can be seen in the [official SHV
 * documentation](https://silicon-heaven.github.io/shv-doc/rpctransportlayer.html#tcp--stream).
 *
 * @param protocol_interface: Protocol transport interface which is used by
 * various protocol layers to read / receive and write / send messages.
 * @param socket: File descriptor of a socket used to both read / receive and
 * write / send messages.
 * @returns New RPC Client object. To free it you need to use @ref
 *   rpcclient_destroy.
 */
rpcclient_t rpcclient_stream_new(
	struct rpcprotocol_interface *protocol_interface, int socket);

/*! Establish a new TCP connection that uses Stream protocol layer.
 *
 * @param location: Location of the TCP server.
 * @param port: Port of the TCP server (commonly 3755).
 * @returns New RPC Client object or `NULL` in case connection couldn't have
 *   been established. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port)
	__attribute__((nonnull));

/*! Establish a new Unix socket connection that uses Stream protocol layer.
 *
 * @param location: Location of the Unix socket.
 * @returns New RPC Client object or `NULL` in case connection couldn't have
 *   been established. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_stream_unix_connect(const char *location)
	__attribute__((nonnull));

/*! Create new RPC Client with Serial protocol layer.
 *
 * The protocol layer description can be seen in the [official SHV
 * documentation](https://silicon-heaven.github.io/shv-doc/rpctransportlayer.html#rs232--serial).
 *
 * @param protocol_interface: Protocol transport interface which is used by
 * various protocol layers to read / receive and write / send messages.
 * @param fd: File descriptor used to both read / receive and write / send
 *   messages.
 * @returns New RPC Client object. To free it you need to use @ref
 *   rpcclient_destroy.
 */
rpcclient_t rpcclient_serial_new(
	struct rpcprotocol_interface *protocol_interface, int fd);

/*! Establish a new connection over terminal device that uses Serial protocol
 * layer.
 *
 * This provides an easy way to open serial console and set it speed. The code
 * changes the mode of the opened terminal device to raw and configures flow
 * control. You can open terminal device on your own if you need a different
 * configuration on the terminal device.
 *
 * @param path: Path to the terminal device.
 * @param baudrate: Speed to be specified as the communication speed on the
 *   terminal device.
 * @returns New RPC Client object or `NULL` in case terminal device open or
 *   configuration failed. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_serial_tty_connect(const char *path, unsigned baudrate)
	__attribute__((nonnull));

/*! Establish a new Unix socket connection that uses Serial protocol layer.
 *
 * The advantage of the Serial protocol layer is that it sends data before they
 * are all generated (because it doesn't have to know the complete message size
 * to do so). That is beneficial in inter-process communication. Once sending
 * process fills socket connection buffer the system can plan the receiving
 * process without anyone of them actually having to keep the whole message in
 * the memory. Operation systems do a good job to set an appropriate size of
 * those buffers to either reduce the memory consumption or to increase the
 * performance.
 *
 * @param location: Location of the Unix socket.
 * @returns New RPC Client object or `NULL` in case connection couldn't have
 *   been established. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_serial_unix_connect(const char *location)
	__attribute__((nonnull));

rpcclient_t rpcclient_serial_tcp_connect(const char *location, int port)
	__attribute__((nonnull));

/*! Perform disconnect and destroy the RPC client object.
 *
 * @param CLIENT: Pointer to the RPC client object.
 */
#define rpcclient_destroy(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_DESTROY))

/*! Check that client is still connected.
 *
 * This doesn't really check if connection is established. It instead checks if
 * RPC client object thinks it is. No communication check happens here.
 *
 * @param CLIENT: The RPC client object.
 * @returns `true` in case client seems to be connected and `false` otherwise.
 */
#define rpcclient_connected(CLIENT) \
	((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_ERRNO) == 0)

/*! Get errno recorded for error in RPC Client.
 *
 * This can be read or write error, they are recorded over each other. In any
 * case it should given you an idea of what caused the failure.
 *
 * @param CLIENT: The RPC client object.
 * @returns Standard error number recorded in reading or writing in RPC Client.
 */
#define rpcclient_errno(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_ERRNO))

/*! Seek for the next message to for reading.
 *
 * Note that this only says that start of the new message was received. The
 * unpack of the message can still block, because the complete message might
 * not be received yet. This is done to allow stream processing of big messages.
 *
 * @param CLIENT: The RPC client object.
 * @returns `true` if next message is received and `false` if not.
 */
#define rpcclient_nextmsg(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_NEXTMSG))

/*! Provides access to the general unpack handle for this client and message.
 *
 * The unpack is allowed to be changed by @ref rpcclient_nextmsg and @ref
 * rpcclient_validmsg and thus make sure that you always refresh your own
 * reference after you call those functions.
 *
 * Unpack always unpacks only a single message. The @ref CPERR_EOF error signals
 * end of the message not end of the connection.
 *
 * @param CLIENT: The RPC client object.
 * @returns @ref cp_unpack_t that can be used to unpack the received message.
 */
#define rpcclient_unpack(CLIENT) (&(CLIENT)->unpack)

/*! Finish reading and validate the received message.
 *
 * This should be called every time at the end of the message receive to
 * validate the received data. Some protocol layers validate messages only
 * after they receive the complete message, not during the protocol. The
 * received bytes can be invalid but still be a valid packing format, or other
 * side can decide to drop the already almost sent message and thus you should
 * always validate message before you proceed to act on the data received.
 *
 * You won't be able to unpack any more items from the message after you call
 * this!
 *
 * @param CLIENT: The RPC client object.
 * @returns `true` if received message was valid and `false` otherwise. Note
 *   that `false` can also mean client disconnect not just invalid message.
 */
#define rpcclient_validmsg(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_VALIDMSG))

/*! Provides access to the general pack handle for this client.
 *
 * The pack is allowed to be changed by @ref rpcclient_sendmsg and @ref
 * rpcclient_dropmsg, make sure that you always refresh your references after
 * you call those functions.
 *
 * You can start packing a new message without any preliminary function. To
 * really send message you need to call @ref rpcclient_sendmsg.
 *
 * @param CLIENT: The RPC client object.
 * @returns @ref cp_pack_t that can be used to pack a message to be sent.
 */
#define rpcclient_pack(CLIENT) (&(CLIENT)->pack)

/*! Send packed message.
 *
 * Based on the link layer implementation it is possible that some data were
 * already sent and thus this is not really the complete send operation. It
 * rather only confirms that sent data were correct. Without calling this the
 * other side won't validate the message successfully, but it can already be
 * processing it. You can consider this as a other side of the @ref
 * rpcclient_validmsg.
 *
 * @param CLIENT: The RPC client object.
 * @returns `true` if message sending was successful, `false` otherwise. The
 *   sending can fail only due to the disconnect.
 */
#define rpcclient_sendmsg(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_SENDMSG))

/*! Drop packed message.
 *
 * This drops not yet sent message and invalidates partially sent one. You can
 * use it instead of @ref rpcclient_sendmsg so you can start sending a different
 * message instead. The primary use case is to replace the existing message with
 * error that was detected when that message was being packed.
 *
 * Based on the protocol layer this causes failure reported by @ref
 * rpcclient_validmsg or it prevents from message to be even sent.
 *
 * @param CLIENT: The RPC client object.
 * @returns `true` if message dropping was successful, `false` otherwise. The
 *   drop might need to send some bytes to inform other side about end of the
 *   message and this operation can as side effect detect the client disconnect.
 */
#define rpcclient_dropmsg(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_DROPMSG))

/*! This provides access to the underlying file descriptor used for reading.
 *
 * This should be used only in poll operations. Do not use other operations on
 * it because that might break internal state consistency of the RPC client.
 *
 * @param CLIENT: The RPC client object.
 * @returns Integer with file descriptor or `-1` if client provides none or lost
 *   connection.
 */
#define rpcclient_pollfd(CLIENT) ((CLIENT)->ctl(CLIENT, RPCC_CTRLOP_POLLFD))

#endif
