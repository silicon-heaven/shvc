/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCLOGGER_H
#define SHV_RPCLOGGER_H
/*! @file
 * Logger used with RPC Client to trace the communication.
 */


#include <stdio.h>
#include <stdbool.h>
#include <shv/cp.h>

/*! RPC Client logger handle. */
typedef struct rpclogger *rpclogger_t;

/*! Create a new RPC Client logger handle.
 *
 * @param f: Stream used to output log lines.
 * @param maxdepth: The output is in CPON format and this allows you to limit
 *   the maximum depth of containers you want to see in the logs. By specifying
 *   low enough number the logger can skip unnecessary data and still show you
 *   enough info about the message so you can recognize it.
 */
rpclogger_t rpclogger_new(FILE *f, unsigned maxdepth)
	__attribute__((nonnull, malloc));

/*! Destroy the existing logger handle.
 *
 * @param logger: Logger handle.
 */
void rpclogger_destroy(rpclogger_t logger);


/*! Lock log for the current thread.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * This must be called before you start using @ref rpclogger_log_item!
 *
 * @param logger: Logger handle.
 * @param in: If message is input (`true`) or output (`false`) message.
 */
void rpclogger_log_lock(rpclogger_t logger, bool in);

/*! Log single item into the logger.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * Do not forget to use @ref rpclogger_log_lock before you start calling this
 * fuinction.
 *
 * For consistency you need to log every single item you unpack otherwise log
 * might be invalid.
 *
 * @param logger: Logger handle.
 * @param item: RPC item to be logged.
 */
void rpclogger_log_item(rpclogger_t logger, const struct cpitem *item)
	__attribute__((nonnull(2)));

/*! Release log lock held by current thread.
 *
 * This unlocks lock locked by @ref rpclogger_log_lock.
 *
 * This is API intended to be called by RPC Client implementations. It is not
 * desirable to call this outside of that context.
 *
 * After this you can't no longer call @ref rpclogger_log_item.
 *
 * @param logger: Logger handle.
 */
void rpclogger_log_unlock(rpclogger_t logger);

#endif
