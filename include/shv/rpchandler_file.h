/* SPDX-License-Identifier: MIT */
#ifndef _RPCHANDLER_FILE_H
#define _RPCHANDLER_FILE_H
#include <obstack.h>
#include <shv/rpchandler.h>

/**
 * RPC Handler that handles file nodes. This handler operates with all files
 * provided by the user with :c:struct:`rpchandler_file_list` structure and
 * implements read/write/append/truncate and validation operations based on
 * the access level for given file. The listing is done via callback provided
 * by :c:struct:`rpchandler_file_cb` structure. The user should implement these
 * callbacks properly.
 *
 * This handles only file nodes and ignore all other nodes. It serves as a
 * simplification for the access to file nodes, allowing the user only to
 * add file node stage and have complete operations on file nodes available.
 * Therefore, the handler is intended to be used alongside user's own handler
 * for standard nodes.
 */

/** This structure provides information about one file node. */
struct rpchandler_file_list {
	/** Path to the SHV node of the file. */
	const char *shv_path;
	/** Path to the file. This is a full path used to perform read/write
	 *  operations on the file.
	 */
	const char *file_path;
	/** The access level given to the file. */
	int access;
};

/** This structure provides callbacks back to the user code.
 *
 *  The handler needs to know the file names, paths and their accesses
 *  to provide complete support for file node. These correspondences/matches
 *  between SHV nodes and physical files have to be specified by the
 *  user. The handler access these through the callback.
 */
struct rpchandler_file_cb {
	/** List all currently present files.
	 *
	 * The function is expected to allocate :c:struct:`rpchandler_file_list`
	 * structure (n * sizeof(rpchandler_file_list) where n is the number of
	 * files), fill it properly and return the pointer to it. Memory is freed by
	 * the handler itself during its deletion..
	 *
	 * :param num: Number of currently present files.
	 * :return: The pointer to :c:struct:`rpchandler_file_list` structure.
	 */
	struct rpchandler_file_list *(*ls)(int *num);
	/** Check for path update for currently present files.
	 *
	 *  There might be a use case where the file's path changes during the
	 *  system runtime (mcuboot ota slots switching for example). The callback
	 *  update_paths is called every time when new rpc message is received and
	 *  provides the possibility to update file path. This does not have to
	 *  be defined, the user can pass NULL instead of this callback and then
	 *  no path update check is performed.
	 *
	 * :param file_path: Path to the file. This path might be updated.
	 * :param shv_path: SHV path to the file. This is not updated, but might
	 *  be used as a reference to determine correct `file_path`.
	 * :return: 0 on success, -1 on failure.
	 */
	int (*update_paths)(const char **file_path, const char *shv_path);
};

/** Object representing RPC File Handler. */
typedef struct rpchandler_file *rpchandler_file_t;

/** Create new RPC File Handler.
 *
 * :param cb: The pointer to :c:struct:`rpchandler_file_cb` structure..
 * :return: RPC File Handler object.
 */
[[gnu::malloc]]
rpchandler_file_t rpchandler_file_new(struct rpchandler_file_cb *cb);

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
 * This is destructor for the object created by :c:func:`rpchandler_file_new`.
 *
 * :param handler: RPC File Handler object.
 */
void rpchandler_file_destroy(rpchandler_file_t handler);

#endif /* _RPCHANDLER_FILE_H */
