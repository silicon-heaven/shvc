/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_H
#define SHV_RPCCLIENT_H
#include <stdbool.h>
#include <errno.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <shv/rpclogger.h>

/**
 * Handle managing a single connection for SHV RPC.
 */

/** Number of seconds that is default idle time before brokers disconnect
 * clients for inactivity.
 */
#define RPC_DEFAULT_IDLE_TIME 180

/** Operations performed by control function :c:member:`rpcclient`.ctrl for RPC
 * Client.
 *
 * To reduce the size of the :c:struct:`rpcclient` structure we use only one
 * function pointer and pass operation as argument. We define macros to hide
 * this but also because control function must be called with pointer to the RPC
 * Client and thus using macros makes is safer and more easy to read.
 */
enum rpcclient_ctrlop {
	/** :c:macro:`rpcclient_destroy` */
	RPCC_CTRLOP_DESTROY,
	/** :c:macro:`rpcclient_disconnect` */
	RPCC_CTRLOP_DISCONNECT,
	/** :c:macro:`rpcclient_reset` */
	RPCC_CTRLOP_RESET,
	/** :c:macro:`rpcclient_errno` and :c:macro:`rpcclient_connected` */
	RPCC_CTRLOP_ERRNO,
	/** :c:macro:`rpcclient_nextmsg` */
	RPCC_CTRLOP_NEXTMSG,
	/** :c:macro:`rpcclient_validmsg` */
	RPCC_CTRLOP_VALIDMSG,
	/** :c:macro:`rpcclient_ignoremsg` */
	RPCC_CTRLOP_IGNOREMSG,
	/** :c:macro:`rpcclient_sendmsg` */
	RPCC_CTRLOP_SENDMSG,
	/** :c:macro:`rpcclient_dropmsg` */
	RPCC_CTRLOP_DROPMSG,
	/** :c:macro:`rpcclient_contrack` */
	RPCC_CTRLOP_CONTRACK,
	/** :c:macro:`rpcclient_pollfd` */
	RPCC_CTRLOP_POLLFD,
};

/** Public definition of RPC Client object.
 *
 * It provides abstraction on top of multiple different protocols providing
 * transfer of SHV RPC messages.
 */
struct rpcclient {
	/** Control function. Do not use directly. */
	int (*ctrl)(struct rpcclient *, enum rpcclient_ctrlop);

	/** Optional query for the peer's name. */
	size_t (*peername)(struct rpcclient *, char *buf, size_t siz);

	/** Unpack function. Please use :c:macro:`rpcclient_unpack` instead. */
	cp_unpack_func_t unpack;

	/** Pack function. Please use :c:macro:`rpcclient_pack` instead. */
	cp_pack_func_t pack;

	/** Loggers used to log messages received by this client. */
	rpclogger_t logger_in;
	/** Loggers used to log message sent by this client. */
	rpclogger_t logger_out;
};

/** Handle used to manage SHV RPC client. */
typedef struct rpcclient *rpcclient_t;


/** Destroy the RPC client object.
 *
 * :param CLIENT: Pointer to the RPC client object.
 */
#define rpcclient_destroy(CLIENT) \
	((void)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_DESTROY))

/** Perform disconnect.
 *
 * :param CLIENT: Pointer to the RPC client object.
 */
#define rpcclient_disconnect(CLIENT) \
	((void)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_DISCONNECT))

/** Perform state reset or reconnect if not connected.
 *
 * :param CLIENT: Pointer to the RPC client object.
 * :return: ``true`` in case client is now connected and ``false`` otherwise.
 *   Investigate ``errno`` to identify the cause.
 */
#define rpcclient_reset(CLIENT) \
	((bool)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_RESET))

/** Check that client is still connected.
 *
 * This doesn't really check if connection is established. It instead checks if
 * RPC client object thinks it is. No communication check happens here.
 *
 * :param CLIENT: The RPC client object.
 * :return: ``true`` in case client seems to be connected and ``false``
 *   otherwise.
 */
#define rpcclient_connected(CLIENT) \
	({ \
		int __errnum = rpcclient_errno(CLIENT); \
		__errnum == 0 || __errnum == EWOULDBLOCK || __errnum == EAGAIN; \
	})

/** Get errno recorded for error in RPC Client.
 *
 * This can be read or write error, they are recorded over each other. In any
 * case it should given you an idea of what caused the failure.
 *
 * :param CLIENT: The RPC client object.
 * :return: Standard error number recorded in reading or writing in RPC Client.
 */
#define rpcclient_errno(CLIENT) ((CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_ERRNO))

/** The value returned from :c:macro:`rpcclient_nextmsg`. */
enum rpcclient_msg_type {
	/** No new message is available for the reading.
	 *
	 * This is to be returned in case polling on :macro:`rpcclient_pollfd`
	 * hinted non-blocking read but no message was actually available for what
	 * ever reason (such as invalid encoding, corrupted data, or unknown type).
	 *
	 * You must expect this to happen even if you are not using polling (thus if
	 * you are using :macro:`rpcclient_nextmsg` as blocking call). In such case
	 * just call :macro:`rpcclient_nextmsg` again.
	 *
	 * This pattern ensures that both polling and blocking usage is possible.
	 * Technically any call to :macro:`rpcclient_nextmsg` when file descriptor
	 * would block on read will block and this enumerator is returned in the
	 * opposite situation.
	 */
	RPCC_NOTHING = 0,
	/** The new message is ready to read. Use macro:`rpcclient_unpack`.
	 *
	 * :macro:`rpcclient_nextmsg` should not be called again before either
	 * :macro:`rpcclient_validmsg` or :macro:`rpcclient_ignoremsg` is called!
	 */
	RPCC_MESSAGE,
	/** Reset received. Reset application state and continue. */
	RPCC_RESET,
	/** Communication error detected. */
	RPCC_ERROR,
};

/** Get the next message.
 *
 * The call will block until some message is available or error is detected. See
 * :enumerator:`RPCC_NOTHING` for more advanced explanation.
 *
 * :param CLIENT: The RPC client object.
 * :return: :c:enum:`rpcclient_msg_type` that signals if message is ready to be
 *   unpacked or not.
 */
#define rpcclient_nextmsg(CLIENT) \
	((enum rpcclient_msg_type)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_NEXTMSG))

/** Provides access to the general unpack handle for this client and message.
 *
 * It can be used only between :c:macro:`rpcclient_nextmsg` and
 * :c:macro:`rpcclient_validmsg` or :c:macro:`rpcclient_ignoremsg` calls.
 *
 * Unpack always unpacks only a single message. The :c:enumerator:`CPERR_EOF`
 * error signals end of the message not end of the connection.
 *
 * :param CLIENT: The RPC client object.
 * :return: :c:type:`cp_unpack_t` that can be used to unpack the received
 *   message.
 */
#define rpcclient_unpack(CLIENT) (&(CLIENT)->unpack)

/** Finish reading and validate the received message.
 *
 * This should be called every time (with exception if
 * :c:func:`rpcclient_ignoremsg` is used) at the end of the message retrieval to
 * validate the received data. Some protocol layers validate messages only after
 * they receive the complete message, not during the protocol. The received
 * bytes can be invalid but still be a valid packing format, or other side can
 * decide to drop the already almost sent message and thus you should always
 * validate message before you proceed to act on the data received.
 *
 * You won't be able to unpack any more items from the message after you call
 * this!
 *
 * :param CLIENT: The RPC client object.
 * :return: ``true`` if received message was valid and ``false`` otherwise. Note
 *   that ``false`` can also mean client disconnect not just invalid message.
 */
#define rpcclient_validmsg(CLIENT) \
	((bool)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_VALIDMSG))

/** Finish reading the received message by just ignoring it.
 *
 * This can be called instead of :c:func:`rpcclient_validmsg` if the message
 * should be ignored. It is mostly the same such as message validation but
 * clients are allowed to perform more efficient data flush as they know that
 * validation is not required.
 *
 * You won't be able to unpack any more items from the message after you call
 * this!
 *
 * :param CLIENT: The RPC client object.
 */
#define rpcclient_ignoremsg(CLIENT) \
	((CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_IGNOREMSG))

/** Provides access to the general pack handle for this client.
 *
 * You can start packing a new message without any preliminary function. To
 * send message fully you need to call :c:macro:`rpcclient_sendmsg`.
 *
 * :param CLIENT: The RPC client object.
 * :return: :c:type:`cp_pack_t` that can be used to pack a message to be sent.
 */
#define rpcclient_pack(CLIENT) (&(CLIENT)->pack)

/** Send packed message.
 *
 * Based on the link layer implementation it is possible that some data were
 * already sent and thus this is not really the complete send operation. It
 * rather only confirms that sent data were correct. Without calling this the
 * other side won't validate the message successfully, but it can already be
 * processing it. You can consider this as a other side of the
 * c:macro:`arpcclient_validmsg`.
 *
 * :param CLIENT: The RPC client object.
 * :return: ``true`` if message sending was successful, ``false`` otherwise. The
 *   sending can fail only due to the disconnect.
 */
#define rpcclient_sendmsg(CLIENT) \
	((bool)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_SENDMSG))

/** Drop packed message.
 *
 * This drops not yet sent message and invalidates partially sent one. You can
 * use it instead of :c:macro:`rpcclient_sendmsg` so you can start sending a
 * different message instead. The primary use case is to replace the existing
 * message with error that was detected when that message was being packed.
 *
 * Based on the protocol layer this causes failure reported by
 * :c:macro:`rpcclient_validmsg` or it prevents from message to be even sent.
 *
 * :param CLIENT: The RPC client object.
 * :return: ``true`` if message dropping was successful, ``false`` otherwise.
 *   The drop might need to send some bytes to inform other side about end of
 *   the message and this operation can as side effect detect the client
 *   disconnect.
 */
#define rpcclient_dropmsg(CLIENT) \
	((bool)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_DROPMSG))

/** This provides access to the underlying file descriptor used for reading.
 *
 * This should be used only in poll operations. Do not use other operations on
 * it because that might break internal state consistency of the RPC client.
 *
 * :param CLIENT: The RPC client object.
 * :return: Integer with file descriptor or ``-1`` if client provides none or
 *   lost connection.
 */
#define rpcclient_pollfd(CLIENT) \
	((int)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_POLLFD))

/** Check if client supports connection tracking of the peer.
 *
 * Connection tracking in this sense tells you if disconnect is propagated
 * as disconnect to the other peer or if it is manifested only as silence on the
 * connection.
 *
 * Based on this the different tools can decide what to do in some cases.
 *
 * :param CLIENT: The RPC client object.
 * :return: Bool signaling if connection has connection tracking capability.
 */
#define rpcclient_contrack(CLIENT) \
	((int)(CLIENT)->ctrl(CLIENT, RPCC_CTRLOP_CONTRACK))

/** Get peer's name.
 *
 * This can be used to identify the peer with its string description.
 *
 * :param client: The RPC client object.
 * :param buf: Pointer to the buffer where string will be stored. You can pass
 *   ``NULL`` alongside with ``size == 0`` and in such case this function only
 *   calculates amount of space required for the string.
 * :param size: Size of the `buf` in bytes.
 * :return: Number of bytes written (excluding the terminating null byte). If
 *   buffer is too small then it returns number bytes it would wrote. This can
 *   be used to detect truncated output due to small buffer.
 */
[[gnu::nonnull(1)]]
static inline size_t rpcclient_peername(rpcclient_t client, char *buf, size_t size) {
	if (client->peername)
		return client->peername(client, buf, size);
	if (buf && size > 0)
		*buf = '\0';
	return 0;
}

#endif
