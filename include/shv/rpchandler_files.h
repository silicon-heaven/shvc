/* SPDX-License-Identifier: MIT */
#ifndef _RPCHANDLER_FILES_H
#define _RPCHANDLER_FILES_H
/*! @file
 * RPC Handler that handles history provided in form of files.
 */

#include <shv/rpchandler.h>

struct rpchandler_files_ops {
	// TODO
};

/*! Object representing RPC Files Handler. */
typedef struct rpchandler_files *rpchandler_files_t;

/*! Create new RPC Files Handler.
 *
 * @returns RPC Files Handler object.
 * @param name: The name of given history log.
 * @param log: The pointer to the logging structure.
 * @param ops: The pointer to the logging dependent operations.
 */
rpchandler_files_t rpchandler_files_new(const char *name, void *log,
	struct rpchandler_files_ops *ops) __attribute__((malloc));

/*! Get RPC Handler stage for this Files Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param files: RPC Files Handler object.
 * @returns Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_files_stage(rpchandler_files_t files);

/*! Free all resources occupied by @ref rpchandler_files_t.
 *
 * This is destructor for the object created by @ref rpchandler_files_new.
 *
 * @param files: RPC Files Handler object.
 */
void rpchandler_files_destroy(rpchandler_files_t files);

#endif /* _RPCHANDLER_FILES_H */
