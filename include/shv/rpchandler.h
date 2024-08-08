/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_H
#define SHV_RPCHANDLER_H
/*! @file
 * The RPC message handler that wraps RPC Client and allows asynchronous access
 * for sending and receiving messages.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>


/*! RPC Handler object.
 *
 * Handler provides a wrapper for receiving messages from the connected peer and
 * process them.
 */
typedef struct rpchandler *rpchandler_t;

/*! Context passed to the callbacks @ref rpchandler_funcs.msg */
struct rpchandler_msg {
	/*! The unpacked meta for the received message.
	 *
	 * Stages can edit this if they wish to influence subsequent stages.
	 */
	struct rpcmsg_meta meta;
	/*! Unpacker that can be used to unpack parameter and result (depending on
	 * the message type).
	 */
	cp_unpack_t unpack;
	/*! Item that must be used for unpacking. */
	struct cpitem *item;
};

/*! Context passed to the callbacks @ref rpchandler_funcs.ls */
struct rpchandler_ls {
	/*! SHV Path that is requested to be listed. */
	const char *const path;
	/*!
	 * There are two uses for call of *ls* method. One when all nodes on given
	 * path are requested and second where you use it to validate existence of
	 * the node in some path. The second usage can sometimes be optimised
	 * because you might not need to call @ref rpchandler_ls_result for every
	 * single node name when we are interested only in one of them. This
	 * function provides you that name.
	 *
	 * Note that if you plan to use it to compare against names one at a time
	 * then you can just use @ref rpchandler_ls_result that does exactly
	 * that. On the other hand if you have some more efficient way (such as
	 * cache) to detecting if node exists then you can speed up the process by
	 * using this function.
	 */
	const char *const name;
};

/*! Context passed to the callbacks @ref rpchandler_funcs.dir */
struct rpchandler_dir {
	/*! SHV Path for which directory is requrested. */
	const char *const path;
	/*! There are two uses for call of *dir* method such as for *ls*. The same
	 * remarks such as for @ref rpchandler_ls.name apply here just with @ref
	 * rpchandler_dir_result function.
	 */
	const char *const name;
};

/*! Context passed to the callbacks @ref rpchandler_funcs.idle */
struct rpchandler_idle {
	/*! Last our activity. This is `CLOCK_MONOTONIC` time. */
	struct timespec last_send;
};

/*! Pointers to the functions called by RPC Handler.
 *
 * This is set of functions needed by handler to handle messages. It is common
 * that implementations define these as constant and provide pointer to it in
 * @ref rpchandler_stage.
 */
struct rpchandler_funcs {
	/*! This is the primary function that is called to handle generic message.
	 *
	 * RPC Handler calls this for received messages in sequence given by stages
	 * array until some function returns `true`.
	 *
	 * This function must investigate @ref rpchandler_msg.meta to see if it
	 * should handle that message. It can immediately return `false` or it can
	 * proceed to handle it. It is prohibited to use @ref rpchandler_msg.unpack
	 * if it returns `false`.
	 *
	 * The handling of the message consist of receiving the message parameters
	 * and validating the message. Handler is allowed to pack response or it
	 * needs to copy @ref rpcmsg_meta.request_id for sending response later on.
	 * It is not allowed to pack any requests!
	 *
	 * The message parameters are parsed with @ref rpchandler_msg.unpack and
	 * @ref rpchandler_msg.item. Before you invoke first @ref cp_unpack you must
	 * make sure that there is actually parameter to unpack with @ref
	 * rpcmsg_has_value (otherwise you can get unpack error even for valid
	 * message). Next you can use unpack functions to unpack parameters any way
	 * you wish.
	 *
	 * The received message needs to be validated after unpack with @ref
	 * rpchandler_msg_valid (internally uses @ref rpcclient_validmsg).
	 *
	 * If you encounter error on receive or sending then you need to still
	 * return `true`. The invalid messages are dropped this way (unpack error
	 * due to the invalid data) and communication error is recorded in RPC
	 * Client that RPC Handler uses.
	 */
	bool (*msg)(void *cookie, struct rpchandler_msg *ctx);
	/*! ls method implementation.
	 *
	 * RPC Handler handles calls to the *ls* methods. It only needs to know
	 * nodes that are present on given path. This function serves that purpose.
	 * It is called every time ls request is being processed to fetch the nodes
	 * for the given path.
	 *
	 * The implementation needs to call @ref rpchandler_ls_result and variants
	 * of that function for every node in the requested path.
	 *
	 * Note that this is attempted only if request is not handled by
	 * `rpchandler_funcs.msg`.
	 */
	void (*ls)(void *cookie, struct rpchandler_ls *ctx);
	/*! dir method implementation.
	 *
	 * RPC Handler handles calls to the *dir* methods. It only needs to know
	 * methods that are associated with node on the given path. This function
	 * serves that purpose. It is called every time dir request is being
	 * processed to fetch the method descriptions for the given path.
	 *
	 * The implementation needs to call @ref rpchandler_dir_result and variants
	 * of that function for every method on the requested path.
	 *
	 * Note that this is attempted only if request is not handled by
	 * `rpchandler_funcs.msg`.
	 */
	void (*dir)(void *cookie, struct rpchandler_dir *ctx);
	/*! The implementation of the idle operations.
	 *
	 * Idle is called when there is no message to be handled. It has two major
	 * use cases:
	 * - It decudes the maximal time we can just wait for message without
	 *   idle beeing called again.
	 * - Can send various messages including requests but with limitation that
	 *   only one message can be send by all stages together at one idle
	 *   invocation.
	 *
	 * This function returns negative number to signal error.
	 */
	int (*idle)(void *cookie, struct rpchandler_idle *ctx);
	/*! The state reset.
	 *
	 * During the communication the reset of the state can be requested by the
	 * peer as well as, if enabled, handler can perform reconnect and in such
	 * case everything must be reverted to the initial state of the
	 * communication.
	 */
	void (*reset)(void *cookie);
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


/*! Create new RPC message handle.
 *
 * @param client RPC client to wrap in the handler. Make sure that you destroy
 *   the RPC handler before you destroy this client instance.
 * @param stages Handling stages that are used to manage messages received from
 *   this client. It is an array that is `NULL` terminated (the @ref
 *   rpchandler_stage.funcs is zero). Stages are referenced as they are and thus
 *   you need to keep this pointer valid for the duration of object existence
 *   (or you can replace it with @ref rpchandler_change_stages).
 * @param limits Limits used for parsing message heads. This is referenced as
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
 * @param rpchandler RPC Handler object.
 */
void rpchandler_destroy(rpchandler_t rpchandler);

/*! Provides access to the current array of stages.
 *
 * @param handler RPC Handler instance.
 * @returns Pointer to the array of states.
 */
const struct rpchandler_stage *rpchandler_stages(rpchandler_t handler)
	__attribute__((nonnull));

/*! This allows you to change the current array of stages.
 *
 * Be aware that this uses locks internally and waits for any concurrent thread
 * indefinitely, this primarily includes the primary handler's thread and thus
 * it is not possible to change stages from withing the @ref rpchandler_funcs
 * functions.
 *
 * @param handler RPC Handler instance.
 * @param stages Stages array to be used for now on with this handler.
 */
void rpchandler_change_stages(rpchandler_t handler,
	const struct rpchandler_stage *stages) __attribute__((nonnull));

/*! Provides access to the wrapped RPC client.
 *
 * Do not use this to send or receive messages! It is provided only for the
 * convenience and status control. The message exchange should always be
 * performed through RPC handler.
 *
 * @param handler RPC handler instance.
 */
rpcclient_t rpchandler_client(rpchandler_t handler) __attribute__((nonnull));

/*! Handle next message.
 *
 * This blocks until the next message is received and fully handled. In general
 * you should not use this directly because you must in between also call @ref
 * rpchandler_idling. These two steps are provided separatelly to allow handler
 * to be included in poll based even loops.
 *
 * @param rpchandler RPC Handler instance.
 * @returns `true` if message handled (even by dropping) and `false` if
 *   connection error encountered in RPC Client. `false` pretty much means that
 *   loop calling repeatedly this should should terminate because we are no
 *   longer able to read or process messages.
 */
bool rpchandler_next(rpchandler_t rpchandler) __attribute__((nonnull));

/*! Call idle callbacks and determine maximal timeout.
 *
 * This should be used in combination with @ref rpchandler_next if you are
 * integrating RPC Handler to some poll based event loop. This function should
 * be called every time there is nothing to be received and determines timeout
 * for which it doesn't have to be called again.
 *
 * @param rpchandler RPC Handler instance.
 * @returns The maximal time during which idle doesn't have to be called (unless
 *   new message is received). It is in milliseconds. Negative number can be
 *   returned that signals error and should be handled same as `false` in @ref
 *   rpchandler_next.
 */
int rpchandler_idling(rpchandler_t rpchandler) __attribute__((nonnull));

/*! Run the RPC Handler loop.
 *
 * This is the most easier way to get RPC Handler up and running. It is the
 * suggested way unless you plan to use some poll based loop and multiple
 * handlers.
 *
 * @param rpchandler RPC Handler instance.
 * @param halt Pointer to the variable that can be set non-zero in the signal
 *   handler to halt the loop on next iteration. You can pass `NULL` if you do
 *   not plan on terminating loop this way. Note that this works only if signal
 *   is delivered to this function's calling thread.
 */
void rpchandler_run(rpchandler_t rpchandler, volatile sig_atomic_t *halt)
	__attribute__((nonnull(1)));

/*! Spawn thread that runs @ref rpchandler_run.
 *
 * It is common to run handler in a separate thread and perform work on primary
 * one. This simplifies this setup by spawning that thread for you.
 *
 * To terminate the thread you can use `pthread_cancel`. Note that it allows
 * cancel only when message is not being handled.
 *
 * @param rpchandler RPC Handler instance.
 * @param thread Pointer to the variable where handle for the pthread is
 *   stored. You can use this to control thread.
 * @param attr Pointer to the pthread attributes or `NULL` for the inherited
 *   defaults.
 * @returns Integer value returned from `pthread_create`.
 */
int rpchandler_spawn_thread(rpchandler_t rpchandler, pthread_t *restrict thread,
	const pthread_attr_t *restrict attr) __attribute__((nonnull(1, 2)));


/// @cond
cp_pack_t _rpchandler_msg_new(rpchandler_t rpchandler) __attribute__((nonnull));
cp_pack_t _rpchandler_impl_msg_new(struct rpchandler_msg *ctx)
	__attribute__((nonnull));
cp_pack_t _rpchandler_idle_msg_new(struct rpchandler_idle *ctx)
	__attribute__((nonnull));
/// @endcond
/*! Start sending new message.
 *
 * This is used to send messages from multiple threads. It ensures that lock is
 * taken and thus only one thread is packing message at the time. The lock is
 * released either with @ref rpchandler_msg_send or with @ref
 * rpchandler_msg_drop.
 *
 * This is implemented as macro that based on the `HANDLER` parameter expands to
 * an appropriate internal method call. Thanks to that it can be used for the
 * same usage in the different contexts:
 * - @ref rpchandler_funcs.msg (@ref rpchandler_msg) can be used to send
 *   responses and signals when request message is received. This function
 *   always provides `NULL` if received message is either signal or response.
 * - @ref rpchandler_funcs.idle (@ref rpchandler_idle) can be used to send one
 *   message per invocation. This way you can send only one message per
 *   invocation and thus subsequent usages will provide `NULL`.
 * - @ref rpchandler_funcs.ls and @ref rpchandler_funcs.dir are disallowed from
 *   sending messages and thus this won't work with @ref rpchandler_ls and @ref
 *   rpchandler_dir.
 * - Any function that is not called from @ref rpchandler_funcs functions can
 *   use this to send messages.
 *
 * @param HANDLER RPC Handler instance or context.
 * @returns Packer you need to use to pack message.
 */
#define rpchandler_msg_new(HANDLER) \
	_Generic((HANDLER), \
		rpchandler_t: _rpchandler_msg_new, \
		struct rpchandler_msg *: _rpchandler_impl_msg_new, \
		struct rpchandler_idle *: _rpchandler_idle_msg_new)(HANDLER)

/// @cond
bool _rpchandler_msg_send(rpchandler_t rpchandler) __attribute__((nonnull));
bool _rpchandler_impl_msg_send(struct rpchandler_msg *ctx)
	__attribute__((nonnull));
bool _rpchandler_idle_msg_send(struct rpchandler_idle *ctx)
	__attribute__((nonnull));
/// @endcond
/*! Send the packed message.
 *
 * This calls @ref rpcclient_sendmsg under the hood and releases the lock taken
 * by @ref rpchandler_msg_new.
 *
 * @param HANDLER RPC Handler instance or context.
 * @returns `true` if send was successful and `false` otherwise.
 */
#define rpchandler_msg_send(HANDLER) \
	_Generic((HANDLER), \
		rpchandler_t: _rpchandler_msg_send, \
		struct rpchandler_msg *: _rpchandler_impl_msg_send, \
		struct rpchandler_idle *: _rpchandler_idle_msg_send)(HANDLER)

/// @cond
bool _rpchandler_msg_drop(rpchandler_t rpchandler) __attribute__((nonnull));
bool _rpchandler_impl_msg_drop(struct rpchandler_msg *ctx)
	__attribute__((nonnull));
bool _rpchandler_idle_msg_drop(struct rpchandler_idle *ctx)
	__attribute__((nonnull));
/// @endcond
/*! Drop the packed message.
 *
 * This calls @ref rpcclient_dropmsg under the hood and releases the lock taken
 * by @ref rpchandler_msg_new.
 *
 * @param HANDLER RPC Handler instance or context.
 * @returns `true` if send was successful and `false` otherwise.
 */
#define rpchandler_msg_drop(HANDLER) \
	_Generic((HANDLER), \
		rpchandler_t: _rpchandler_msg_drop, \
		struct rpchandler_msg *: _rpchandler_impl_msg_drop, \
		struct rpchandler_idle *: _rpchandler_idle_msg_drop)(HANDLER)


/*! Utility combination of @ref rpchandler_msg_new and @ref rpcmsg_pack_request.
 *
 * @param HANDLER RPC Handler instance or context.
 * @param PATH SHV path to the node the method we want to request is associated
 *   with.
 * @param METHOD name of the method we request to call.
 * @param REQUEST_ID request identifier. Thanks to this number you can
 *   associate response with requests.
 * @returns @ref cp_pack_t or `NULL` on error.
 */
#define rpchandler_msg_new_request(HANDLER, PATH, METHOD, REQUEST_ID) \
	({ \
		cp_pack_t __pack = rpchandler_msg_new(HANDLER); \
		if (__pack) \
			rpcmsg_pack_request(__pack, (PATH), (METHOD), (REQUEST_ID)); \
		__pack \
	})

/*! Utility combination of @ref rpchandler_msg_new and @ref
 * rpcmsg_pack_request_void.
 *
 * @param HANDLER RPC Handler instance or context.
 * @param PATH SHV path to the node the method we want to request is associated
 *   with.
 * @param METHOD name of the method we request to call.
 * @param REQUEST_ID request identifier. Thanks to this number you can
 *   associate response with requests.
 * @returns @ref cp_pack_t or `NULL` on error.
 */
#define rpchandler_msg_new_request_void(HANDLER, PATH, METHOD, REQUEST_ID) \
	({ \
		cp_pack_t __pack = rpchandler_msg_new(HANDLER); \
		if (__pack) \
			rpcmsg_pack_request_void(__pack, (PATH), (METHOD), (REQUEST_ID)); \
		__pack != NULL \
	})

#endif
