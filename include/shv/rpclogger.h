/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCLOGGER_H
#define SHV_RPCLOGGER_H
/*! @file
 * Logger used with RPC Client to trace the communication.
 *
 * Logger is implemented as a buffer that stores part of the message and outputs
 * it at once either when message is flushed or if buffer overflows. You can
 * hookup to this your own logging solution.
 */

#include <stdbool.h>
#include <shv/cp.h>

/*! RPC Client logger handle. */
typedef struct rpclogger *rpclogger_t;

/*! Callback definition for RPC client logger.
 *
 * @param line: The line to be outputted to the logs.
 */
typedef void (*rpclogger_func_t)(const char *line);

/*! Create a new RPC Client logger handle.
 *
 * @param callback: Function called when line should be outputed.
 * @param prefix: The prefix to be added before every line.
 * @param bufsiz: Size of the buffer for this logger.
 * @param maxdepth: The output is in CPON format and this allows you to limit
 *   the maximum depth of containers you want to see in the logs. By specifying
 *   low enough number the logger can skip unnecessary data and still show you
 *   enough info about the message so you can recognize it.
 * @returns New logger handle or `NULL` in case `0` was passed to `maxdepth`.
 */
rpclogger_t rpclogger_new(rpclogger_func_t callback, const char *prefix,
	size_t bufsiz, unsigned maxdepth) __attribute__((nonnull(1), malloc));

/*! Destroy the existing logger handle.
 *
 * @param logger: Logger handle.
 */
void rpclogger_destroy(rpclogger_t logger);


/*! Log single item into the logger.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * For consistency you need to log every single item you unpack otherwise log
 * might be invalid.
 *
 * @param logger: Logger handle.
 * @param item: RPC item to be logged.
 */
void rpclogger_log_item(rpclogger_t logger, const struct cpitem *item)
	__attribute__((nonnull));

enum rpclogger_end_type {
	RPCLOGGER_ET_VALID,
	RPCLOGGER_ET_INVALID,
	RPCLOGGER_ET_UNKNOWN,
};

/*! Log the end of the message.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * This signals logger end of a message that consist of previous logged items.
 * This calls @ref rpclogger_log_flush. This flushes items buffered so far to
 * the configured output.
 *
 * @param logger: Logger handle.
 * @param tp: If the items logged so far were valid message or not.
 */
void rpclogger_log_end(rpclogger_t logger, enum rpclogger_end_type tp)
	__attribute__((nonnull));

/*! Flush the buffered output.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * This flushes items buffered so far to the configured output.
 *
 * @param logger: Logger handle.
 */
void rpclogger_log_flush(rpclogger_t logger) __attribute__((nonnull));

/*! Predefined callback that outputs line to the stderr.
 *
 * This is provided because it is a common way to do logging.
 *
 * @param line: See @ref rpclogger_func_t.
 */
void rpclogger_func_stderr(const char *line);

#endif
