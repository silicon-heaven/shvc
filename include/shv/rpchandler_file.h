/* SPDX-License-Identifier: MIT */
#ifndef _RPCHANDLER_FILE_H
#define _RPCHANDLER_FILE_H
#include <obstack.h>
#include <shv/rpchandler.h>

/**
 * RPC Handler that handles file nodes. This handler operates with all files
 * provided by the user with :c:struct:`rpchandler_file_node` structure and
 * implements read/write/append/truncate and validation operations based on
 * the access level for given file. It also supports directories and
 * provides all files in the given directory.
 *
 * This handles only file nodes and ignore all other nodes. It serves as a
 * simplification for the access to file nodes, allowing the user only to
 * add file node stage and have complete operations on file nodes available.
 * Therefore, the handler is intended to be used alongside user's own handler
 * for standard nodes.
 */

/** Flag enabling read functionality in :var:`rpchandler_file_node.access`. */
#define RPCFILE_ACCESS_READ (1U << 0)
/** Flag enabling validation functionality in
 * :var:`rpchandler_file_node.access`.
 *
 * Provides CRC32/SHA1 validation methods.
 *
 * This will be auto-enabled when :macro`RPCFILE_ACCESS_READ` is enabled but can
 * be used without it as well.
 */
#define RPCFILE_ACCESS_VALIDATION (1U << 1)
/** Flag enabling write functionality in :var:`rpchandler_file_node.access`. */
#define RPCFILE_ACCESS_WRITE (1U << 2)
/** Flag enabling truncate functionality in :var:`rpchandler_file_node.access`.
 *
 * This allows write outside of the initial file size boundary and resize the
 * boundary.
 *
 * This will auto-enable :macro:`RPCFILE_ACCESS_WRITE`.
 */
#define RPCFILE_ACCESS_TRUNCATE (1U << 3)
/** Flag enabling append functionality in :var:`rpchandler_file_node.access`.
 *
 * This can be enabled regardless of :macro:`RPCFILE_ACCESS_WRITE` and
 * :macro:`RPCFILE_ACCESS_TRUNCATE`. The append is always write after the
 * current file boundary.
 */
#define RPCFILE_ACCESS_APPEND (1U << 4)


/** This structure provides information about one file node. */
struct rpchandler_file_node {
	/** Path to the SHV node of the file. */
	const char *shv_path;
	/** Path to the file. This is a full path used to perform read/write
	 *  operations on the file.
	 *
	 * .. NOTE::
	 *    Only regular files are supported.
	 */
	const char *file_path;
	/** The access level given to the file. */
	unsigned access;
	/** True if the entry is not a single file but rather a directory.
	 * In this case the handler will provide all files in this directory
	 * and will create a directory node based on ``shv_path`` member.
	 *
	 * For example an entry with `shv_path` set as `LOGS` will provide node
	 * `LOGS` with `ls` and `dir` methods and standard file nodes under SHV
	 * `LOGS/` path for all files provided in the ``file_path`` directory.
	 *
	 * Access level set by ``access`` are aplied to all files in the directory.
	 *
	 * .. NOTE::
	 *    Only files with depth 1 are provided.
	 */
	bool isdir;
};

/** Object representing RPC File Handler. */
typedef struct rpchandler_file *rpchandler_file_t;

/** Create new RPC File Handler.
 *
 * :param entries: ``NULL`` terminated array of :c:struct:`rpchandler_file_node`
 *  structure providing the files and directories for the handler.
 * :return: RPC File Handler object.
 */
[[gnu::malloc, gnu::nonnull]]
rpchandler_file_t rpchandler_file_new(const struct rpchandler_file_node **entries);

/** Get RPC Handler stage for this Records Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * :param handler: RPC File Handler object.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_file_stage(rpchandler_file_t handler);

/** Free all resources occupied by :c:type:`rpchandler_file_t`.
 *
 * This is destructor for the object created by
 * :c:func:`rpchandler_file_new`.
 *
 * :param handler: RPC File Handler object.
 */
void rpchandler_file_destroy(rpchandler_file_t handler);

#endif /* _RPCHANDLER_FILE_H */
