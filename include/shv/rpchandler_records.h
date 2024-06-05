/* SPDX-License-Identifier: MIT */
#ifndef _RPCHANDLER_RECORDS_H
#define _RPCHANDLER_RECORDS_H
#include <obstack.h>
#include <shv/rpchandler.h>
#include <shv/rpchistory.h>
/*! @file
 * RPC Handler that handles history provided in form of block based records.
 */

/*! This structure provides callbacks to logging frame.
 *
 * In general, SHV does not know anything about logging framework
 * and vice versa. This allows great variaty of SHV usage with
 * different logging frameworks regardless of their structures. There
 * however has to be some way to obtain records and available index
 * range from the logging library. This is done through RPC Handler
 * callbacks. These should be assigned by the device that should also
 * function as an intermediate between SHV and logging library.
 */
struct rpchandler_records_ops {
	/*! Read record at given index from the log and pack it with @ref cp_pack_t
	 *  structure.
	 *
	 * @param log: The pointer to the logging structure.
	 * @param index: Index of the record to be packed.
	 * @param pack: Packing structure @ref cp_pack_t.
	 * @param obstack: The pointer to obstack.
	 * @returns `false` if packing encounters failure and `true` otherwise.
	 */
	bool (*pack_record)(
		void *log, int index, cp_pack_t pack, struct obstack *obstack);
	/*! Obtains the lowest valid ID, the highest valid ID and keep record span.
	 *
	 * @param log: The pointer to the logging structure.
	 * @param min: The lowest valid record index.
	 * @param max: The highest valid record index.
	 * @param span: Record keep span.
	 * @returns `false` on failure and `true` otherwise.
	 */
	bool (*get_index_range)(
		void *log, uint64_t *min, uint64_t *max, uint64_t *span);
};

/*! Object representing RPC Records Handler. */
typedef struct rpchandler_records *rpchandler_records_t;

/*! Create new RPC Records Handler.
 *
 * @param name: The name of given history log.
 * @param log: The pointer to the logging structure.
 * @param ops: The pointer to the logging dependent operations.
 * @returns RPC Records Handler object.
 */
rpchandler_records_t rpchandler_records_new(const char *name, void *log,
	struct rpchandler_records_ops *ops) __attribute__((malloc));

/*! Get RPC Handler stage for this Records Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param records: RPC Records Handler object.
 * @returns Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_records_stage(rpchandler_records_t records);

/*! Free all resources occupied by @ref rpchandler_records_t.
 *
 * This is destructor for the object created by @ref rpchandler_records_new.
 *
 * @param records: RPC Records Handler object.
 */
void rpchandler_records_destroy(rpchandler_records_t records);

#endif /* _RPCHANDLER_RECORDS_H */
