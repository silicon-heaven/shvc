/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCLOGGER_H
#define SHV_RPCLOGGER_H
#include <stdbool.h>
#include <shv/cp.h>

/**
 * Logger used with RPC Client to trace the communication.
 *
 * Logger is implemented as a buffer that stores part of the message and outputs
 * it at once either when message is flushed or if buffer overflows. You can
 * hookup to this your own logging solution.
 */

/** RPC Client logger handle. */
typedef struct rpclogger *rpclogger_t;

/** Definition of functions for RPC logger.
 *
 * The pointer to this structure is passed to :c:func:`rpclogger_new` function.
 * It defines the functions necessary to perform the logging.
 */
struct rpclogger_funcs {
	/** Logging callback that performs the actual logging.
	 *
	 * :param line: The line to be outputted to the logs.
	 */
	void (*log)(const char *line);
	/**	Informs the logger whether it should log or not.
	 *
	 * Logging is performed based on :c:func:`rpclogger_func_t` callback. It is
	 * possible the current level of logging is not enabled by the system
	 * and we can optimize the code by skipping CPON packing at all. This
	 * function is called before every logging is performed.
	 *
	 * :return: ``true`` if logging should be performed, ``false`` otherwise.
	 */
	bool (*would_log)(void);
};

/** Create a new RPC Client logger handle.
 *
 * :param callbacks: Pointer to :c:struct:`rpclogger_funcs` defining
 * 	callbacks for the RPC logger.
 * :param prefix: The prefix to be added before every line. The provided pointer
 * 	doesn't have be valid after this call finishes because content is copied.
 * :param bufsiz: Size of the buffer for this logger.
 * :param maxdepth: The output is in CPON format and this allows you to limit
 * 	the maximum depth of containers you want to see in the logs. By specifying
 *	low enough number the logger can skip unnecessary data and still show you
 * 	enough info about the message so you can recognize it.
 * :return: New logger handle or ``NULL`` in case ``0`` was passed to
 * 	``maxdepth``.
 */
[[gnu::nonnull(1), gnu::malloc]]
rpclogger_t rpclogger_new(const struct rpclogger_funcs *funcs,
	const char *prefix, size_t bufsiz, unsigned maxdepth);

/** Destroy the existing logger handle.
 *
 * :param logger: Logger handle.
 */
void rpclogger_destroy(rpclogger_t logger);

/** Log single item into the logger.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * For consistency you need to log every single item you unpack otherwise log
 * might be invalid.
 *
 * You most likely want to call this from :c:var:`rpcclient.pack` and
 * :c:var:`rpcclient.unpack` implementation.
 *
 * :param logger: Logger handle.
 * :param item: RPC item to be logged.
 */
[[gnu::nonnull(2)]]
void rpclogger_log_item(rpclogger_t logger, const struct cpitem *item);

/** Log received reset.
 *
 * This is API intended to be called by RPC Client implementations. It is
 * not desirable to call this outside of that context.
 *
 * It is handled the same way as :c:func:`rpclogger_log_item` item is and it is
 * expected that :c:func:`rpclogger_log_end` will be called afterwards.
 *
 * :param logger: Logger handle.
 */
void rpclogger_log_reset(rpclogger_t logger);

/** Identification of the message end type.
 *
 * This controls hinting at the end of the line to identify if this message.
 */
enum rpclogger_end_type {
	/** This is standard message end. The message was correctly received or
	 * sent.
	 */
	RPCLOGGER_ET_VALID,
	/** This informs logger that messages was identified as invalid. This can be
	 * due to transmission error (only for receive) or because message sending
	 * was aborted.
	 *
	 * Some client implementations might not even send a byte until message gets
	 * confirmed for sending but that is not know to the logger and is still
	 * logged as invalid message.
	 */
	RPCLOGGER_ET_INVALID,
	/** This is used in case the message state is unknown. We just don't know if
	 * the message was valid or not.
	 */
	RPCLOGGER_ET_UNKNOWN,
};

/** Log the end of the message.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * This signals logger end of a message that consist of previous logged items.
 * This flushes items buffered so far to the configured output.
 *
 * In case of message sending you most likely want to call this from
 * implementations of :c:macro:`rpcclient_sendmsg` with
 * :c:enumerator:`RPCLOGGER_ET_VALID` and from :c:func:`rpcclient_dropmsg` with
 * :c:enumerator:`RPCLOGGER_ET_INVALID`.
 *
 * In case of message retrieval you most likely want to call this from
 * implementations of :c:macro:`rpcclient_validmsg` where based on the result
 * you use either :c:enumerator:`RPCLOGGER_ET_VALID` or
 * :c:enumerator:`RPCLOGGER_ET_INVALID`. You should also call this from
 * :c:macro:`rpcclient_nextmsg` implementation with
 * :c:enumerator:`RPCLOGGER_ET_UNKNOWN` to ensure that any previous invalidate
 * message is correctly terminated in the logging.
 *
 * @param logger Logger handle.
 * @param tp If the items logged so far were valid message or not.
 */
void rpclogger_log_end(rpclogger_t logger, enum rpclogger_end_type tp);

/** Flush the buffered output.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * This flushes items buffered so far to the configured output.
 *
 * :param logger: Logger handle.
 */
void rpclogger_log_flush(rpclogger_t logger);

/** Default RPC logger functions for syslog logging. */
extern struct rpclogger_funcs rpclogger_syslog_funcs;
/** Default RPC logger functions for stderr logging. */
extern struct rpclogger_funcs rpclogger_stderr_funcs;

#endif
