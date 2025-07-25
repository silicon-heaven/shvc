/* SPDX-License-Identifier: MIT */
#ifndef _RPCHANDLER_HISTORY_H
#define _RPCHANDLER_HISTORY_H
#include <obstack.h>
#include <shv/rpchandler.h>
#include <shv/rpchistory.h>

/**
 * RPC Handler that handles history provided in form of block based and
 * file based records.
 *
 * .. WARNING::
 *   File based logging is currently not implemented.
 */

/** This structure provides the description of one records based log.
 *
 * .. WARNING::
 *   File based logging is currently not implemented,
 */
struct rpchandler_history_files {
	/** Logging framework specific cookie. This may represent the logging
	 * facility for example.
	 */
	void *cookie;
	/** The name of the log. This will be concatated with .records/ and used as
	 * a SHV path.
	 */
	const char *name;
};

/** This structure provides the description of one records based log.
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
	/** Logging framework specific cookie. This may represent the logging
	 * facility for example.
	 */
	void *cookie;
	/** The name of the log. This will be concatated with ``.records/`` and used
	 * as a SHV path.
	 *
	 * .. WARNING::
	 *   The current implementation does not support ``/`` character in the
	 *   names.
	 */
	const char *name;
	/** Read records from start index to start + num index and pack them with
	 *  :c:type:`cp_pack_t` packer.
	 *
	 * The implementation may pack less records than specified if these are
	 * not present in the logging facility.
	 *
	 * :param cookie: Logging framework specific cookie, refer to cookie.
	 * :param start: Index of the first record to be packed.
	 * :param num: Number of records to be packed.
	 * :param pack: Packing structure :c:type:`cp_pack_t`.
	 * :param obstack: The pointer to obstack.
	 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
	 */
	bool (*pack_records)(void *cookie, int start, int num, cp_pack_t pack,
		struct obstack *obstack);
	/** Obtains the lowest valid ID, the highest valid ID and keep record span.
	 *
	 * :param cookie: Logging framework specific cookie, refer to
	 *   :c:member:`cookie`.
	 * :param min: The lowest valid record index.
	 * :param max: The highest valid record index.
	 * :param span: Record keep span.
	 * :return: ``false`` on failure and ``true`` otherwise.
	 */
	bool (*get_index_range)(
		void *cookie, uint64_t *min, uint64_t *max, uint64_t *span);
};

struct rpchandler_history_facilities;

/** Definitions of function for RPC History handler
 *
 * This defines necessary interface to support getLog and getSnapshot
 * method packing in RPC History Handler.
 */
struct rpchandler_history_funcs {
	/** Callback to user defined implementation of getLog method response pack.
	 *
	 * This is a callback to the user/application defined function that
	 * obtains the records from the logging facilities based on parameters
	 * passed in :c:func:`rpchistory_getlog_request` argument and packs the
	 * records. The application may use
	 * :c:func:`rpchistory_getlog_response_pack_begin` and
	 * :c:func:`rpchistory_getlog_response_pack_end` function calls to pack
	 * the response. The begining and the end of the list is already implemented
	 * in the RPC History Handler.
	 *
	 * The function should fail only if it is not possible to obtain records
	 * from the log due to some internal error (for example the log is not
	 * able to correctly represent time). Asking for time span or path
	 * not present should just return without packing any records and thus
	 * the response will be an empty list.
	 *
	 * :param facilities: The pointer to
	 *   :c:struct:`rpchandler_history_facilities` structure. This provides an
	 *   access to the logging facilities.
	 * :param request: The pointer to unpacked getLog request parameters.
	 * :param pack: Packing structure :c:type:`cp_pack_t`.
	 * :param obstack: The pointer to RPC Handler obstack.
	 * :param path: RPC path where the method was called.
	 * :return: True on success, false on failure.
	 */
	bool (*pack_getlog)(struct rpchandler_history_facilities *facilities,
		struct rpchistory_getlog_request *request, cp_pack_t pack,
		struct obstack *obstack, const char *path);
	/** Callback to user defined implementation of getSnapshot method response
	 *
	 * The function should fail only if it is not possible to obtain records
	 * from the log due to some internal error (for example the log is not
	 * able to correctly represent time). Asking for time span or path
	 * not present should just return without packing any records and thus
	 * the response will be an empty list.
	 *
	 * :param facilities: The pointer to
	 *   :c:struct:`rpchandler_history_facilities` structure. This provides an
	 *   access to the logging facilities.
	 * :param request: The pointer to unpacked getSnapshot request parameters.
	 * :param pack: Packing structure :c:type:`cp_pack_t`.
	 * :param obstack: The pointer to RPC Handler obstack.
	 * :param path: RPC path where the method was called.
	 * :return: True on success, false on failure.
	 */
	bool (*pack_getsnapshot)(struct rpchandler_history_facilities *facilities,
		struct rpchistory_getsnapshot_request *request, cp_pack_t pack,
		struct obstack *obstack, const char *path);
};

/** This structure provides the pointers to the arrays of records and
 * file based logging facilities.
 *
 * This is passed as an argument to :c:func:`rpchandler_history_new` function.
 * This way the user application may fill the handler with required log
 * facilities.
 */
struct rpchandler_history_facilities {
	/** The array of pointers to :c:struct:`rpchandler_history_records`
	 * structure. The array has to be ``NULL`` terminated!
	 */
	struct rpchandler_history_records **records;
	/** The array of pointers to :c:struct:`rpchandler_history_files` structure.
	 * The array has to be ``NULL`` terminated!
	 */
	struct rpchandler_history_files **files;
	/** Pointer to :c:struct:`rpchandler_history_funcs` defining ``getLog`` and
	 * ``getSnapshot`` packing functions.
	 */
	struct rpchandler_history_funcs *funcs;
};

/** Object representing RPC Records Handler. */
typedef struct rpchandler_history *rpchandler_history_t;

/** Create new RPC History Handler.
 *
 * This handler implements the entire history framework. It combines
 * block based histories (records) and file based histories (files) - NOT YET
 * IMPLEMENTED and provides the implementation of getLog interface.
 *
 * :param facilities: The pointer to :c:struct:`rpchandler_history_facilities`
 *   structure. This provides the information about histories to be used in the
 *   framework, it is an array terminated by ``NULL``.
 * :param signals: Null terminated array with the paths to signals being
 *   recorded by all facilities. These paths are present in the virtual tree
 *   under ``./history`` node and contains getLog method.
 * :return: RPC Records Handler object.
 */
[[gnu::malloc, gnu::nonnull]]
rpchandler_history_t rpchandler_history_new(
	struct rpchandler_history_facilities *facilities, const char **signals);

/** Get RPC Handler stage for this History Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * :param history: RPC History Handler object.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_history_stage(rpchandler_history_t history);

/** Free all resources occupied by :c:type:`rpchandler_history_t`.
 *
 * This is destructor for the object created by
 * :c:func:`rpchandler_history_new`.
 *
 * :param history: RPC History Handler object.
 */
void rpchandler_history_destroy(rpchandler_history_t history);

#endif /* _RPCHANDLER_HISTORY_H */
