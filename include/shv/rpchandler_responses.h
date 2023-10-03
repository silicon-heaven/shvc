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

/*! Wait for the response to be received.
 *
 * This blocks execution of the thread for up to the given timeout.
 *
 * @param respond: Respond object to wait for.
 * @param receive: Pointer to the variable where pointer to the receive
 *   structure is placed in. This structure provides you with reference you need
 *   to receive message just like in @ref rpchandler_funcs.msg.
 * @param meta: Pointer to the variable where pointer to the meta structure is
 *   placed in. This structure contains info about received message such as its
 *   type or if it caries an error.
 * @param timeout: Number of seconds we wait before we stop waiting.
 * @returns `true` if response received or `false` otherwise.
 */
bool rpcresponse_waitfor(rpcresponse_t respond, struct rpcreceive **receive,
	struct rpcmsg_meta **meta, int timeout) __attribute__((nonnull));

/*! Validate the response message.
 *
 * This also frees the RPC Response from RPC Responses Handler.
 *
 * @param respond: Respond object to wait for.
 * @returns `true` if message is valid and `false` otherwise.
 */
bool rpcresponse_validmsg(rpcresponse_t respond) __attribute__((nonnull));


#endif
