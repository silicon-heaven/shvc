/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_H
#define SHV_RPCHANDLER_H
/*! @file
 * The RPC message handler that wraps RPC Client and allows asynchronous access
 * for sending and receiving messages.
 */

#include "shv/cp.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdarg.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include <shv/rpcnode.h>


/*! Handle passed to the message handling functions @ref rpchandler_funcs.msg.
 *
 * It provides access to functionality needed to receive message and optionally
 * also to send single message back. The full access is intentionally limited
 * to reduce the time it takes to receive message. If you need to pass some
 * handle to the message handling function then you might not be doing what you
 * should.
 */
struct rpcreceive {
	/*! Unpack handle for reading parameter of the received message. */
	cp_unpack_t unpack;
	/*! Item to be used to read parameter of the received message. */
	struct cpitem item;
	/*! Obstack instance used to store values in **meta**. You are free to use
	 * it in the message handling function. All that is allocated in it will be
	 * freed after message is fully handled.
	 */
	struct obstack obstack;
};

/*! Handle passed to the ls handling functions @ref rpchandler_funcs.ls. */
struct rpchandler_ls_ctx;

/*! Handle passed to the dir handling functions @ref rpchandler_funcs.dir. */
struct rpchandler_dir_ctx;

/*! Pointers to the functions called by RPC Handler.
 *
 * This is set of functions needed by handler to handle messages. It is common
 * that implementations define these as constant and provide pointer to it in
 * @ref rpchandler_stage.
 */
struct rpchandler_funcs {
	/*! This is the primary function that is called to handle generic message.
	 *
	 * RPC Handler calls this for received requests in sequence given by stages
	 * array until some function returns `true`.
	 *
	 * This function must investigate **meta** to see if it should handle that
	 * message. It can immediately return `false` or it can proceed to handle
	 * it. It is prohibited to touch the **receive** if it returns `false`.
	 *
	 * The handling of the message consist of receiving the message parameters
	 * and validating the message. Handler is allowed to pack response or it
	 * needs to copy **meta** for sending response later on. It is not allowed
	 * to pack any other message to the RPC Client this handler manages to
	 * minimize the time handler is blocked.
	 *
	 * The message parameters are parsed with @ref rpcreceive.unpack and @ref
	 * rpcreceive.item. Before you invoke first @ref cp_unpack you must make
	 * sure that there is actually parameter to unpack with @ref
	 * rpcreceive_has_param (otherwise you can get unpack error even for valid
	 * message). Next you can use unpack functions to unpack parameters any way
	 * you wish.
	 *
	 * The received message needs to be validated after unpack with @ref
	 * rpcreceive_validmsg (internally uses @ref rpcclient_validmsg).
	 *
	 * If you encounter error on receive or sending then you need to still
	 * return `true`. The invalid messages are dropped this way (unpack error
	 * due to the invalid data) and communication error is recorded in RPC
	 * Client that RPC Handler uses.
	 */
	bool (*msg)(void *cookie, struct rpcreceive *receive,
		const struct rpcmsg_meta *meta);
	/*! ls method implementation.
	 *
	 * RPC Handler handles all calls to the *ls* methods, It only needs to know
	 * nodes that are present on given path. This function serves that purpose.
	 * It is called every time ls request is being processed to fetch the nodes
	 * for the given path.
	 *
	 * The implementation needs to call @ref rpchandler_ls_result and variants
	 * of that function for every node in the requested path.
	 */
	void (*ls)(void *cookie, const char *path, struct rpchandler_ls_ctx *ctx);
	/*! dir method implementation.
	 *
	 * @ref rpchandler_destroy
	 *
	 * RPC Handler handles all calls to the *dir* methods, It only needs to know
	 * methods that are associated with node on the given path. This function
	 * serves that purpose. It is called every time dir request is being
	 * processed to fetch the method descriptions for the given path.
	 *
	 * The implementation needs to call @ref rpchandler_dir_result and variants
	 * of that function for every method on the requested path.
	 */
	void (*dir)(void *cookie, const char *path, struct rpchandler_dir_ctx *ctx);
};

/*! Single stage in the RPC Handler.
 *
 * RPC Handler works with array of these stages and handler implementations
 * commonly provide function returning this structure.
 */
struct rpchandler_stage {
	/*! Pointer to the @ref rpchandler_funcs with pointer to the function
	 * implementing the specific functionality.
	 */
	const struct rpchandler_funcs *funcs;
	/*! Cookie passed to the functions when they are called. It allows access to
	 * some context and thus function structure can be generic for multiple
	 * instances where only cookies are different.
	 */
	void *cookie;
};

/*! RPC Handler object.
 *
 * Handler provides a wrapper for receiving messages from the connected peer and
 * process them.
 */
typedef struct rpchandler *rpchandler_t;


/*! Create new RPC message handle.
 *
 * @param client: RPC client to wrap in the handler. Make sure that you destroy
 *   the RPC handler before you destroy this client instance.
 * @param stages: Handling stages that are used to manage messages received from
 *   this client. It is an array that is `NULL` terminated (the @ref
 *   rpchandler_stage.funcs is zero). Stages are referenced as they are and thus
 *   you need to keep this pointer valid for the duration of object existence
 *   (or you can replace it with @ref rpchandler_change_stages).
 * @param limits: Limits used for parsing message heads. This is referenced as
 *   it is and this you need to keep this pointer valid for the duration of
 *   object existence.
 * @returns New RPC handler instance.
 */
rpchandler_t rpchandler_new(rpcclient_t client,
	const struct rpchandler_stage *stages, const struct rpcmsg_meta_limits *limits)
	__attribute__((nonnull(1, 2), malloc));

/*! Free all resources occupied by @ref rpchandler_t object.
 *
 * This is destructor for the object created by @ref rpchandler_new.
 *
 * The RPC Client that Handle manages is not destroyed nor disconnected.
 *
 * @param rpchandler: RPC Handler object.
 */
void rpchandler_destroy(rpchandler_t rpchandler);

/*! This allows you to change the current array of stages.
 *
 * You can change stages anytime you want but it will have no effect on the
 * currently handled messages. The primary issue in multithreading with this is
 * that you can't just free the previous stages array, you must wait for the
 * current message to be processed and with @ref rpchandler_spawn_thread that is
 * not easily known. If you are using loop with @ref rpchandler_next, then you
 * can be sure that between calls to that function there is no message being
 * processed.
 *
 * @param handler: RPC Handler instance.
 * @param stages: Stages array to be used for now on with this handler.
 */
void rpchandler_change_stages(rpchandler_t handler,
	const struct rpchandler_stage *stages) __attribute__((nonnull));

/*! Handle next message.
 *
 * This blocks until the next message is received and fully handled.
 *
 * @param rpchandler: RPC Handler instance.
 * @returns `true` if message handled (even by dropping) and `false` if
 *   connection error encountered in RPC Client. `false` pretty much means that
 *   loop calling repeatedly this should should terminate because we are no
 *   longer able to read messages.
 */
bool rpchandler_next(rpchandler_t rpchandler) __attribute__((nonnull));

/*! RPC Handler errors */
enum rpchandler_error {
	/*! Disconnect is reported by RPC Client. */
	RPCHANDLER_DISCONNECT,
	/*! Detected too long inactivity. */
	RPCHANDLER_TIMEOUT,
};

/*! Run the RPC Handler loop.
 *
 * This is the most easier way to get RPC Handler up and running. It is the
 * suggested way unless you plan to use some poll based loop and multiple
 * handlers. The single handler can live pretty easily in it this thread.
 *
 * Loop performs the following actions:
 * * Wait for data to be read from client and calls @ref rpchandler_next.
 * * On inactivity longer than half of the @ref RPC_DEFAULT_IDLE_TIME it calls
 * **onerr** with @ref RPCHANDLER_TIMEOUT. Then it continues executing (fail
 * was either resolved or this will lead to disconnect by other side).
 * * On disconnect it calls **onerr** with @ref RPCHANDLER_DISCONNECT. It
 * continues only if client is connected afterwards, otherwise it terminates the
 * loop.
 *
 * @param rpchandler: RPC Handler instance.
 * @param onerr: Callback called when some error is detected. It allows you to
 *   act on that error and possibly clear it before return from callback to
 *   continue the loop. Callback gets the RPC Handler object and RPC Client
 *   object it wraps. You can pass `NULL` in which case default function is used
 *   that ignores disconnects and sends ping requests on timeout.
 */
void rpchandler_run(rpchandler_t rpchandler,
	void (*onerr)(rpchandler_t, rpcclient_t, enum rpchandler_error))
	__attribute__((nonnull(1)));

/*! Spawn thread that runs @ref rpchandler_run.
 *
 * It is common to run handler in separate thread and perform work on primary
 * one. This simplifies this setup by spawning that thread for you.
 *
 * @param rpchandler: RPC Handler instance.
 * @param onerr: Passed to @ref rpchandler_run.
 * @param thread: Pointer to the variable where handle for the pthread is
 *   stored. You can use this to control thread.
 * @param attr: Pointer to the pthread attributes or `NULL` for the inherited
 *   defaults.
 * @returns Integer value returned from `pthread_create`.
 */
int rpchandler_spawn_thread(rpchandler_t rpchandler,
	void (*onerr)(rpchandler_t, rpcclient_t, enum rpchandler_error),
	pthread_t *restrict thread, const pthread_attr_t *restrict attr)
	__attribute__((nonnull(1, 2)));


/*! Get next unused request ID.
 *
 * @param rpchandler: RPC Handler instance.
 * @returns New request ID.
 */
int rpchandler_next_request_id(rpchandler_t rpchandler) __attribute__((nonnull));


/*! Start sending new message.
 *
 * This is used to send messages from multiple threads. It ensures that lock is
 * taken and thus only one thread is packing message at the time. The lock is
 * released either with @ref rpchandler_msg_send or with @ref
 * rpchandler_msg_drop.
 *
 * @param rpchandler: RPC Handler instance.
 * @returns Packer you need to use to pack message.
 */
cp_pack_t rpchandler_msg_new(rpchandler_t rpchandler) __attribute__((nonnull));

/*! Send the packed message.
 *
 * This calls @ref rpcclient_sendmsg under the hood and releases the lock taken
 * by @ref rpchandler_msg_new.
 *
 * @param rpchandler: RPC Handler instance.
 * @returns `true` if send was successful and `false` otherwise.
 */
bool rpchandler_msg_send(rpchandler_t rpchandler) __attribute__((nonnull));

/*! Drop the packed message.
 *
 * This calls @ref rpcclient_dropmsg under the hood and releases the lock taken
 * by @ref rpchandler_msg_new.
 *
 * @param rpchandler: RPC Handler instance.
 * @returns `true` if send was successful and `false` otherwise.
 */
bool rpchandler_msg_drop(rpchandler_t rpchandler) __attribute__((nonnull));


/*! Query if received message has parameter to unpack.
 *
 * You should use this right before you start unpacking parameters to ensure
 * that there are some.
 *
 * @param receive: Receive handle passed to @ref rpchandler_funcs.msg.
 * @returns `true` if there is parameter to unpack and `false` if there are not.
 */
__attribute__((nonnull)) static inline bool rpcreceive_has_param(
	struct rpcreceive *receive) {
	return receive->item.type != CPITEM_CONTAINER_END;
}

/*! Validate the received message.
 *
 * This calls @ref rpcclient_validmsg under the hood. You must call it to
 * validate that message is valid. Do not act on the received data if this
 * function returns `false`.
 *
 * @param receive: Receive handle passed to @ref rpchandler_funcs.msg.
 * @returns `true` if message is valid and `false` otherwise.
 */
bool rpcreceive_validmsg(struct rpcreceive *receive) __attribute__((nonnull));

/*! Start packing response.
 *
 * Make sure that you call this only if you received request and only after you
 * called @ref rpcreceive_validmsg.
 *
 * @param receive: Receive handle passed to @ref rpchandler_funcs.msg.
 * @returns Packer used to pack response message.
 */
cp_pack_t rpcreceive_response_new(struct rpcreceive *receive)
	__attribute__((nonnull));

/*! Send packed response message.
 *
 * @param receive: Receive handle passed to @ref rpchandler_funcs.msg.
 * @returns `true` if send is successful and `false` otherwise.
 */
bool rpcreceive_response_send(struct rpcreceive *receive) __attribute__((nonnull));

/*! Drop packed response message.
 *
 * You can pack a different message instead.
 *
 * @param receive: Receive handle passed to @ref rpchandler_funcs.msg.
 * @returns `true` if send is successful and `false` otherwise.
 */
bool rpcreceive_response_drop(struct rpcreceive *receive) __attribute__((nonnull));


/*! Add result of `ls`.
 *
 * This is intended to be called only from @ref rpchandler_funcs.ls functions.
 *
 * The same **name** can be specified multiple times but it is added only once.
 * The reason for this is because in the tree the same node might belong to
 * multiple handlers and thus multiple handlers would report it and because they
 * can't know about each other we just need to prevent from duplicates to be
 * added.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.ls.
 * @param name: Name of the node.
 */
void rpchandler_ls_result(struct rpchandler_ls_ctx *context, const char *name)
	__attribute__((nonnull));

/*! The variant of the @ref rpchandler_ls_result with constant string.
 *
 * @ref rpchandler_dir_result needs to duplicate **name** to filter out
 * duplicates. That is not required if string is constant or can be considered
 * such for the time of ls method handling. This function simply takes pointer
 * to the **name** and uses it internally in RPC Handler without copying it.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.ls.
 * @param name: Name of the node (must be pointer to memory that is kept valid
 *   for the duration of ls method handler).
 */
void rpchandler_ls_result_const(struct rpchandler_ls_ctx *context,
	const char *name) __attribute__((nonnull));

/*! The variant of the @ref rpchandler_ls_result with name generated from format
 * string.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.ls.
 * @param fmt: Format string used to generate node name.
 * @param args: List of variable arguments used in format string.
 */
void rpchandler_ls_result_vfmt(struct rpchandler_ls_ctx *context,
	const char *fmt, va_list args) __attribute__((nonnull));

/*! The variant of the @ref rpchandler_ls_result with name generated from format
 * string.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.ls.
 * @param fmt: Format string used to generate node name.
 */
__attribute__((nonnull)) static inline void rpchandler_ls_result_fmt(
	struct rpchandler_ls_ctx *context, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	rpchandler_ls_result_vfmt(context, fmt, args);
	va_end(args);
}

/*! Access name of the node ls should validate that it exists.
 *
 * There are two uses for call of *ls* method. One when all nodes on given path
 * are requested and second where you use it to validate existence of the node
 * in some path. The second usage can sometimes be optimised because you might
 * not need to call @ref rpchandler_ls_result for every single node name when
 * we are interested only in one of them. This function provides you that name.
 *
 * Note that if you plan to use it to compare against names one at a time then
 * you can just use @ref rpchandler_ls_result that does exactly that. On the
 * other hand if you have some more efficient way to detecting if node exists
 * than you can speed up the process by using this function.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.ls.
 * @returns Pointer to the node name or `NULL` if all nodes should be listed.
 */
const char *rpchandler_ls_name(struct rpchandler_ls_ctx *context)
	__attribute__((nonnull));

/*! Add result of `dir`.
 *
 * This is intended to be called only from @ref rpchandler_funcs.dir functions.
 *
 * Compared to the @ref rpchandler_ls_result is not filtering duplicates,
 * because handlers should not have duplicate methods between each other. There
 * is also no need for function like @ref rpchandler_ls_result_const because
 * **name** is used immediately without need to preserve it.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.dir.
 * @param name: Name of the method.
 * @param signature: Signature the method has.
 * @param flags: Combination of flags for the method.
 * @param access: Minimal access level needed to access this method.
 * @param description: Optional description of the method.
 */
void rpchandler_dir_result(struct rpchandler_dir_ctx *context, const char *name,
	enum rpcnode_dir_signature signature, int flags, enum rpcmsg_access access,
	const char *description) __attribute__((nonnull(1, 2)));

/*! Access name of the method dir should query info about.
 *
 * The same case as in @ref rpchandler_ls_name but instead of validating dir
 * with method name is used to query info about a specific method. The
 * effective use is the same: You can implement more efficient algorithm for
 * searching for method based on the name and that way increase performance.
 *
 * @param context: Context passed to the @ref rpchandler_funcs.ls.
 * @returns Pointer to the method name or `NULL` if all methods should be
 * listed.
 */
const char *rpchandler_dir_name(struct rpchandler_ls_ctx *context)
	__attribute__((nonnull));

#endif
