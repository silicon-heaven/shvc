/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_H
#define SHV_RPCCLIENT_H
/*! @file
 * Handle managing a single connection for SHV RPC.
 */

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <shv/rpcurl.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <shv/rpclogger.h>

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

	/*! Unpack function. Please use @ref rpcclient_unpack instead. */
	cp_unpack_func_t unpack;
	/*! The last time we received message.
	 *
	 * This is used by RPC Broker to detect inactive clients.
	 */
	struct timespec last_receive;

	/*! Pack function. Please use @ref rpcclient_pack instead. */
	cp_pack_func_t pack;
	/*! Last time we sent message.
	 *
	 * This is used by RPC clients to detect that thay should perform some
	 * activity to stay connected to the RPC Broker.
	 */
	struct timespec last_send;

	/*! Logger used to log communication happenning with this client.
	 *
	 * Be aware that log implementation is based on the locks and thus all
	 * logged clients under the same logger are serialized and only one client
	 * runs at a time. If you have a bit complex setup deadlocks can be easily
	 * encountered.
	 *
	 * You can change logger while client is handling some message but you must
	 * ensure that previous logger stays valid for some time so the running
	 * function can finish with the old log. Thew new log will take effect only
	 * with next message processed.
	 */
	rpclogger_t logger;
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

/*! Perform login on the newly established connection.
 *
 * @param client: The RPC client object.
 * @param opts: RPC login options. It is common to get this directly from the
 *   RPC URL.
 * @param errmsg: Pointer to the string pointer to which we optionally place the
 *   error message sent by the SHV Broker. Otherwise the pointer to the string
 *   is set to `NULL`. The error message is allocated using `malloc` and you
 *   must free it after use. You can pass `NULL` if you are not interested in
 *   error message at all.
 * @returns `true` if login is successful and `false` otherwise. The login can
 *   fail either due to the communication error or because of SHV Broker
 *   declining the login for any reason. The difference can be detected with
 *   **errmsg** and @ref rpcclient_connected.
 */
bool rpcclient_login(rpcclient_t client, const struct rpclogin_options *opts,
	char **errmsg) __attribute__((nonnull(1, 2)));


/*! Create new RPC Client with Stream transport layer.
 *
 * The transport layer description can be seen in the [official SHV
 * documentation](https://silicon-heaven.github.io/shv-doc/rpctransportlayer.html#tcp--stream).
 *
 * @param readfd: File descriptor used for reading / receiving.
 * @param writefd: File descriptor used for writing / sending.
 * @returns New RPC Client object. To free it you need to use @ref
 *   rpcclient_destroy.
 */
rpcclient_t rpcclient_stream_new(int readfd, int writefd);

/*! Establish a new TCP connection that uses Stream transport layer.
 *
 * @param location: Location of the TCP server.
 * @param port: Port of the TCP server (commonly 3755).
 * @returns New RPC Client object or `NULL` in case connection couldn't have
 *   been established. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port)
	__attribute__((nonnull));

/*! Establish a new Unix socket connection that uses Stream transport layer.
 *
 * @param location: Location of the Unix socket.
 * @returns New RPC Client object or `NULL` in case connection couldn't have
 *   been established. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_stream_unix_connect(const char *location)
	__attribute__((nonnull));

/*! Create new RPC Client with Serial transport layer.
 *
 * The transport layer description can be seen in the [official SHV
 * documentation](https://silicon-heaven.github.io/shv-doc/rpctransportlayer.html#rs232--serial).
 *
 * @param fd: File descriptor used to both read / receive and write / send
 *   messages.
 * @returns New RPC Client object. To free it you need to use @ref
 *   rpcclient_destroy.
 */
rpcclient_t rpcclient_serial_new(int fd);

/*! Establish a new connection over terminal device that uses Serial transport
 * layer.
 *
 * This provides an easy way to open serial console and set it speed. The code
 * changes the mode of the opened terminal device to raw and configures flow
 * control. You can open terminal device on your own if you need a different
 * configuration on the terminal device.
 *
 * @param path: Path to the terminal device.
 * @param speed: Speed to be specified as the communication speed on the
 *   terminal device.
 * @returns New RPC Client object or `NULL` in case terminal device open or
 *   configuration failed. You can investigate the `errno` to identify why that
 *   might have been the case.
 */
rpcclient_t rpcclient_serial_tty_connect(const char *path, unsigned speed)
	__attribute__((nonnull));

/*! Establish a new Unix socket connection that uses Serial transport layer.
 *
 * The advantage of the Serial transport layer is that it sends data before they
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

/*! Wait for the next message to be ready for reading.
 *
 * Note that this only says that start of the new message was received. The
 * unpack of the message can still block, because the complete message might
 * not be received yet. This is done to allow stream processing of big messages.
 *
 * @param CLIENT: The RPC client object.
 * @returns `true` if next message is received and `false` if client is no
 * longer connected.
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
 * validate the received data. Some transport layers validate messages only
 * after they receive the complete message, not during the transport. The
 * received bytes can be invalid but still be a valid packing format, or other
 * side can decide to drop the already almost sent message and thus you should
 * always parse the whole message and validate it before you proceed to act on
 * the data received.
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
 * Based on the transport layer this causes failure reported by @ref
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

/*! Calculate maximum sleep before some message needs to be sent.
 *
 * This time is the number of seconds until the half of the disconnect delay
 * seconds is reached.
 *
 * @param client: Client handle.
 * @param idle_time: Number of seconds before
 * @returns number of seconds before you should send some message.
 */
static inline int rpcclient_maxsleep(rpcclient_t client, int idle_time) {
	struct timespec t;
	assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
	int period = idle_time / 2;
	if (t.tv_sec > client->last_send.tv_sec + period)
		return 0;
	return period - t.tv_sec + client->last_send.tv_sec;
}

/*! Update @ref rpcclient.last_receive to current time.
 *
 * This is available mostly for implementation of additional RPC Clients.
 * Existing implementations are already calling this internally in the
 * appropriate places and thus you do not need to call it on your own.
 *
 * @param client: Client handle.
 */
static inline void rpcclient_last_receive_update(struct rpcclient *client) {
	clock_gettime(CLOCK_MONOTONIC, &client->last_receive);
}

/*! Update @ref rpcclient.last_send to current time.
 *
 * This is available mostly for implementation of additional RPC Clients.
 * Existing implementations are already calling this internally in the
 * appropriate places and thus you do not need to call it on your own.
 *
 * @param client: Client handle.
 */
static inline void rpcclient_last_send_update(struct rpcclient *client) {
	clock_gettime(CLOCK_MONOTONIC, &client->last_send);
}

#endif
