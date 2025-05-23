/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_RESPONSES_H
#define SHV_RPCHANDLER_RESPONSES_H
#include <shv/rpchandler.h>
#include <shv/rpchandler_impl.h>

/**
 * RPC Handler that delivers responses to the callbacks waiting for them.
 */


/** Object representing RPC Responses Handler. */
typedef struct rpchandler_responses *rpchandler_responses_t;

/** Create new RPC REsponses Handler.
 *
 * :return: RPC Responses Handler object.
 */
[[gnu::malloc]]
rpchandler_responses_t rpchandler_responses_new(void);

/** Free all resources occupied by :c:type:`rpchandler_responses_t`.
 *
 * This is destructor for the object created by
 * :c:func:`rpchandler_responses_new`.
 *
 * :param responses: RPC Responses Handler object.
 */
void rpchandler_responses_destroy(rpchandler_responses_t responses);

/** Get RPC Handler stage for this Responses Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * :param responses: RPC Responses Handler object.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_responses_stage(
	rpchandler_responses_t responses);

/** Object representing a single response in RPC Responses Handler. */
typedef struct rpcresponse *rpcresponse_t;

/** The prototype for the callbacks that are called by response handler.
 *
 * It is called only if request ID matches for the response message this
 * callback got registered.
 *
 * You should first use :c:func:`rpcmsg_has_value` to check if response even has
 * any parameter. You can unpack it if so after that.
 *
 * You must call :c:func:`rpchandler_msg_valid` to fully handle the message. Do
 * not act upon received response if :c:func:`rpchandler_msg_valid` returns
 * ``false``.
 *
 * The returned boolean signals if associated expectation should be removed from
 * responses handler or not. In general you want to return result of
 * :c:func:`rpchandler_msg_valid`. In case that function returns ``true`` then
 * no subsequent message with this request ID will be received and in case of
 * ``false`` this might be an abandoned response and new response will be sent
 * instead and thus we should keep waiting for it.
 *
 * :param ctx: This is :c:var:`rpchandler_funcs.msg` context.
 * :param cookie: This is the pointer provided as context when response
 *   expectation is being registered.
 */
typedef bool (*rpcresponse_callback_t)(struct rpchandler_msg *ctx, void *cookie);

/** Register that new response will be received.
 *
 * You should call this before you send (ref rpchandler_msg_send) the
 * message to prevent race condition where response might have been already
 * received.
 *
 * :param responses: RPC Responses Handler object.
 * :param request_id: Request ID of response to be waited for.
 * :param func: The callback function that is called when response is received.
 * :param cookie: Pointer passed as an extra argument to the callback function.
 * :return: Object you need to use to reference to this response.
 */
[[gnu::nonnull]]
rpcresponse_t rpcresponse_expect(rpchandler_responses_t responses,
	int request_id, rpcresponse_callback_t func, void *cookie);

/** Provide request ID of the given response.
 *
 * :param response: Response object the request ID should be provided for.
 * :return: The request ID.
 */
[[gnu::nonnull]]
int rpcresponse_request_id(rpcresponse_t response);

/** Discard the expectation of the response.
 *
 * The usage is when you send request but later decide that you no longer need
 * the response for whatever reason.
 *
 * :param response: The response to discard.
 */
void rpcresponse_discard(rpcresponse_t response);

/** Wait for the response to be received.
 *
 * This blocks execution of the thread for up to the given timeout.
 *
 * On success the response is automatically freed and thus after this function
 * returns `true` the response pointer will no longer be valid. At the same time
 * you can be sure that callback was executed.
 *
 * :param response: Response object to wait for.
 * :param timeout: Number of seconds we wait before we stop waiting.
 * :return: ``true`` if response received or ``false`` otherwise (timeout
 *   encountered).
 */
[[gnu::nonnull]]
bool rpcresponse_waitfor(rpcresponse_t response, int timeout);

/** Validate the response message.
 *
 * This also frees the RPC Response from RPC Responses Handler.
 *
 * :param response: Respond object to wait for.
 * :return: ``true`` if message is valid and ``false`` otherwise.
 */
[[gnu::nonnull]]
bool rpcresponse_validmsg(rpcresponse_t response);


// clang-format off
/** Macro that helps you pack request with parameter, registers response
 * expectation and sends the request.
 *
 * The statement (or compound statement) must follow that packs parameter. The
 * :c:type:`cp_pack_t` is available in this statement in variable ``packer``.
 *
 * The implementation is using ``for`` loops but only in a single iteration
 * (thus your code is executed only once). You can use ``continue`` to skip rest
 * of the compound statement. To terminate packing for whatever reason you can
 * use ``break``. Do not use ``return`` or you must call
 * :c:func:`rpchandler_msg_drop` before you use ``return`` to prevent deadlock.
 *
 * The example usage:
 *
 * .. code-block:: c
 *
 *   rpcresponse_t response;
 *   rpcresponse_send_request(handler, responses_handler, "test/device/prop", "set", response) {
 *   	cp_pack_int(packer, 42);
 *   }
 *   struct rpcreceive *receive;
 *   const struct rpcmsg_meta *meta;
 *   if (response && rpcresponse_waitfor(response, &receive, &meta, 300)) {
 *   	// Parse result value here
 *   }
 *
 * :param handler: The :c:type:`rpchandler_t` object associated with ``responses``.
 * :param responses: The :c:type:`rpchandler_responses_t` object.
 * :param path: Null-terminated string with SHV path.
 * :param method: Null-terminated script with method name.
 * :param uid: User's  ID to be added to the request. It can be ``NULL`` and in
 *   such case User ID won't be part of the message.
 * :param func: The callback passed to :c:func:`rpcresponse_expect`.
 * :param ctx: The pointer passed to the callback.
 * :param response: The variable name where :c:type:`rpcresponse_t` object will
 *   be stored. This is set to ``NULL`` if packing or sending fails.
 */
// clang-format on
#define rpcresponse_send_request( \
	handler, responses, path, method, uid, func, ctx, response) \
	for (int request_id = rpchandler_next_request_id(handler), __ = 1; __; ({ \
			 if (__) { \
				 rpchandler_msg_drop(handler); \
				 response = NULL; \
				 __--; \
			 } \
		 })) \
		for (cp_pack_t packer = rpchandler_msg_new_request( \
				 (handler), (path), (method), (uid), request_id); \
			packer; ({ \
				cp_pack_container_end(packer); \
				packer = NULL; \
				response = rpcresponse_expect((responses), request_id, func, ctx); \
				__ = 0; \
				if (!rpchandler_msg_send(handler)) { \
					rpcresponse_discard(responses, response); \
					response = NULL; \
				} \
			}))

/** Send request without any parameter and provide response object for it.
 *
 * This packs a new request message and establishes expectation for the
 * response.
 *
 * The used functions in this order: :c:func:`rpchandler_msg_new`,
 * :c:func:`rpcmsg_request_id`, :c:func:`rpcmsg_pack_request_void`,
 * :c:func:`rpcresponse_expect`, and :c:func:`rpchandler_msg_send`.
 *
 * :param handler: The RPC Handler responses object is registered in.
 * :param responses: The RPC Responses Handler object to be used.
 * :param path: The SHV path to the node with method to be called.
 * :param method: The method name to be called.
 * :param uid: User's  ID to be added to the request. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * :param func: The callback passed to :c:func:`rpcresponse_expect`.
 * :param ctx: The pointer passed to the callback.
 * :return: Object referencing response to this request or `NULL` in case
 *   request sending failed.
 */
[[gnu::nonnull]]
rpcresponse_t rpcresponse_send_request_void(rpchandler_t handler,
	rpchandler_responses_t responses, const char *path, const char *method,
	const char *uid, rpcresponse_callback_t func, void *ctx);


#endif
