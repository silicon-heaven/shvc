/* SPDX-License-Identifier: MIT */
#ifndef _RPCHANDLER_HISTORY_H
#define _RPCHANDLER_HISTORY_H
#include <obstack.h>
#include <shv/rpchandler.h>
#include <shv/rpchistory.h>
/*! @file
 * RPC Handler that handles history provided in form of block based and
 * file based records.
 *
 * WARNING: File based logging is currently not implemented.
 */

/*! This structure provides the description of one records based log.
 *
 * WARNING: File based logging is currently not implemented,
 */
struct rpchandler_history_files {
	/*! Logging framework specific cookie. This may represent the logging
	 * facility for example.
	 */
	void *cookie;
	/*! The name of the log. This will be concatated with .records/ and used as
	 * a SHV path.
	 */
	const char *name;
};

/*! This structure provides the description of one records based log.
 *
 * In general, SHV does not know anything about logging framework
 * and vice versa. This allows great variaty of SHV usage with
 * different logging frameworks regardless of their structures. There
 * however has to be some way to obtain records and available index
 * range from the logging library. This is done through  callbacks present in
 * this structure. These should be assigned by the device that should also
 * function as an intermediate between SHV and logging library.
 */
struct rpchandler_history_records {
	/*! Logging framework specific cookie. This may represent the logging
	 * facility for example.
	 */
	void *cookie;
	/*! The name of the log. This will be concatated with .records/ and used as
	 * a SHV path.
	 *
	 * WARNING: The current implementation does not support / character in the
	 * names.
	 */
	const char *name;
	/*! Read record at given index from the log and pack it with @ref cp_pack_t
	 *  structure.
	 *
	 * @param cookie: Logging framework specific cookie, refer to @ref cookie.
	 * @param index: Index of the record to be packed.
	 * @param pack: Packing structure @ref cp_pack_t.
	 * @param obstack: The pointer to obstack.
	 * @returns `false` if packing encounters failure and `true` otherwise.
	 */
	bool (*pack_record)(
		void *cookie, int index, cp_pack_t pack, struct obstack *obstack);
	/*! Obtains the lowest valid ID, the highest valid ID and keep record span.
	 *
	 * @param cookie: Logging framework specific cookie, refer to @ref cookie.
	 * @param min: The lowest valid record index.
	 * @param max: The highest valid record index.
	 * @param span: Record keep span.
	 * @returns `false` on failure and `true` otherwise.
	 */
	bool (*get_index_range)(
		void *cookie, uint64_t *min, uint64_t *max, uint64_t *span);
};

/*! This structure provides the pointers to the arrays of records and
 * file based logging facilities.
 *
 * This is passed as an argument to @ref rpchandler_history_new function. This
 * way the user application may fill the handler with required log facilities.
 */
struct rpchandler_history_facilities {
	/*! The array of pointers to @ref rpchandler_history_records structure. The
	 * array has to be NULL terminated! */
	struct rpchandler_history_records **records;
	/*! The array of pointers to @ref rpchandler_history_files structure. The
	 * array has to be NULL terminated! */
	struct rpchandler_history_files **files;
};

/*! Object representing RPC Records Handler. */
typedef struct rpchandler_history *rpchandler_history_t;

/*! Create new RPC History Handler.
 *
 * This handler implements the entire history framework. It combines
 * block based histories (records) and file based histories (files) - NOT YET
 * IMPLEMENTED and is supposed to provide the future implementation of getLog
 * interface.
 *
 * @param facilities: The pointer to @ref rpchandler_history_facilities
 * structure. This provides the information about histories to be used in the
 * framework, it is an array terminated by NULL.
 * @returns RPC Records Handler object.
 */
rpchandler_history_t rpchandler_history_new(
	struct rpchandler_history_facilities *facilities) __attribute__((malloc));

/*! Get RPC Handler stage for this History Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param history: RPC History Handler object.
 * @returns Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_history_stage(rpchandler_history_t history)
	__attribute__((nonnull));

/*! Free all resources occupied by @ref rpchandler_history_t.
 *
 * This is destructor for the object created by @ref rpchandler_history_new.
 *
 * @param history: RPC History Handler object.
 */
void rpchandler_history_destroy(rpchandler_history_t history);

#endif /* _RPCHANDLER_HISTORY_H */
