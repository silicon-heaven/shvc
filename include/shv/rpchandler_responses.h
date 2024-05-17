/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_RESPONSES_H
#define SHV_RPCHANDLER_RESPONSES_H
/*! @file
 * RPC Handler that delivers responses to the thread waiting for them.
 *
 * RPC Handler is implemented to handle all received messages in the primary
 * thread. If you want get data on some other thread then you must synchronize
 * these two threads and pass all parameters needed to receive the message. This
 * is potentially a common action and for that reason this handler stage is
 * provided to give you this functionality.
 */

#include <shv/rpchandler.h>


/*! Object representing RPC Responses Handler. */
typedef struct rpchandler_responses *rpchandler_responses_t;

/*! Create new RPC REsponses Handler.
 *
 * @returns RPC Responses Handler object.
 */
rpchandler_responses_t rpchandler_responses_new(void) __attribute__((malloc));

/*! Free all resources occupied by @ref rpchandler_responses_t.
 *
 * This is destructor for the object created by @ref rpchandler_responses_new.
 *
 * @param responses: RPC Responses Handler object.
 */
void rpchandler_responses_destroy(rpchandler_responses_t responses);

/*! Get RPC Handler stage for this Responses Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * @param responses: RPC Responses Handler object.
 * @returns Stage to be used in array of stages for RPC Handler.
 */
struct rpchandler_stage rpchandler_responses_stage(
	rpchandler_responses_t responses) __attribute__((nonnull));

/*! Object representing a single response in RPC Responses Handler. */
typedef struct rpcresponse *rpcresponse_t;

/*! Register that new response will be received.
 *
 * You should call this right before you send (ref rpchandler_msg_send) the
 * message to prevent race condition where response might have been already
 * received.
 *
 * @param responses: RPC Responses Handler object.
 * @param request_id: Request ID of response to be waited for.
 * @returns Object you need to use to reference to this response.
 */
rpcresponse_t rpcresponse_expect(rpchandler_responses_t responses, int request_id)
	__attribute__((nonnull));

/*! Discard the expectation of the response.
 *
 * The usage is when you send request but later decide that you no longer need
 * the response for whatever reason.
 *
 * @param responses:
 * @param reponse: The response to discard.
 */
void rpcresponse_discard(rpchandler_responses_t responses, rpcresponse_t response)
	__attribute__((nonnull(1)));

// clang-format off
/*! Macro that helps you pack request with parameter, registers response
 * expectation and sends the request.
 *
 * The statement (or compound statement) must follow that packs parameter. The
 * @ref cp_pack_t is available in this statement in variable `packer`.
 *
 * The implementation is using `for` loops but only in a single iteration (thus
 * your code is executed only once). You can use `continue` to skip rest of the
 * compound statement. To terminate packing for whatever reason you can use
 * `break`. Do not use `return` or you must call @ref rpchandler_msg_drop before
 * you use `return` to prevent deadlock.
 *
 * The example usage:
 * ```c
 * rpcresponse_t response;
 * rpcresponse_send_request(handler, responses_handler, "test/device/prop", "set", response) {
 * 	cp_pack_int(packer, 42);
 * }
 * struct rpcreceive *receive;
 * const struct rpcmsg_meta *meta;
 * if (response && rpcresponse_waitfor(response, &receive, &meta, 300)) {
 * 	// Parse result value here
 * }
 * ```
 *
 * @param handler: The @ref rpchandler_t object associated with `responses`.
 * @param responses: The @ref rpchandler_responses_t object.
 * @param path: Null terminated string with SHV path.
 * @param method: Null terminated script with method name.
 * @param response: The variable name where @ref rpcresponse_t object will be
 *   stored. This is set to `NULL` if packing or sending fails.
 */
// clang-format on
#define rpcresponse_send_request(handler, responses, path, method, response) \
	for (int request_id = rpchandler_next_request_id(handler), __ = 1; __; ({ \
			 if (__) { \
				 rpchandler_msg_drop(handler); \
				 __--; \
			 } \
		 })) \
		for (cp_pack_t packer = ({ \
				 cp_pack_t __packer = rpchandler_msg_new(handler); \
				 if (!rpcmsg_pack_request(__packer, (path), (method), request_id)) { \
					 rpchandler_msg_drop(handler); /* Release the lock */ \
					 response = NULL; \
					 __packer = NULL; \
				 } \
				 __packer; \
			 }); \
			 packer; ({ \
				 cp_pack_container_end(packer); \
				 packer = NULL; \
				 response = rpcresponse_expect((responses), request_id); \
				 __ = 0; \
				 if (!rpchandler_msg_send(handler)) { \
					 rpcresponse_discard(responses, response); \
					 response = NULL; \
				 } \
			 }))

/*! Send request without any parameter and provide response object for it.
 *
 * This packs a new request message and establishes expectation for the
 * response.
 *
 * The used functions in this order: @ref rpchandler_msg_new, @ref
 * rpchandler_next_request_id, @ref rpcmsg_pack_request_void, @ref
 * rpcresponse_expect, and @ref rpchandler_msg_send.
 *
 * @param handler: The RPC Handler responses object is registered in.
 * @param responses: The RPC Responses Handler object to be used.
 * @param path: The SHV path to the node with method to be called.
 * @param method: The method name to be called.
 * @returns Object referencing response to this request or `NULL` in case
 *   request sending failed.
 */
rpcresponse_t rpcresponse_send_request_void(rpchandler_t handler,
	rpchandler_responses_t responses, const char *path, const char *method)
	__attribute__((nonnull));

/*! Provide request ID of the given response.
 *
 * @param response: Response object the request ID should be provided for.
 * @returns The request ID.
 */
int rpcresponse_request_id(rpcresponse_t response) __attribute__((nonnull));

/*! Wait for the response to be received.
 *
 * This blocks execution of the thread for up to the given timeout.
 *
 * @param response: Response object to wait for.
 * @param receive: Pointer to the variable where pointer to the receive
 *   structure is placed in. This structure provides you with reference you need
 *   to receive message just like in @ref rpchandler_funcs.msg.
 * @param meta: Pointer to the variable where pointer to the meta structure is
 *   placed in. This structure contains info about received message such as its
 *   type or if it caries an error.
 * @param timeout: Number of seconds we wait before we stop waiting.
 * @returns `true` if response received or `false` otherwise (timeout
 *   encountered).
 */
bool rpcresponse_waitfor(rpcresponse_t response, struct rpcreceive **receive,
	const struct rpcmsg_meta **meta, int timeout) __attribute__((nonnull));

/*! Validate the response message.
 *
 * This also frees the RPC Response from RPC Responses Handler.
 *
 * @param response: Respond object to wait for.
 * @returns `true` if message is valid and `false` otherwise.
 */
bool rpcresponse_validmsg(rpcresponse_t response) __attribute__((nonnull));


#endif
