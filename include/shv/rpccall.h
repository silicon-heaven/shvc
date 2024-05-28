/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_CALL_H
#define SHV_RPCHANDLER_CALL_H
/*! @file
 * Tool to easily call methods over SHV RPC. It depends on @ref
 * rpchandler_responses_t.
 *
 * Calling methods in SHV RPC can get pretty complex pretty fast. The primary
 * reason for that is because any message can get lost and thus we at least have
 * to try multiple times. The second reason for complexity is that message can
 * be aborted even in mid delivery and thus we can't act on received data
 * immediately (rather only after the whole message is received).
 *
 * The selected solution is to combine generic steps with call specific ones
 * through callback function. The call specific code is provided in callback
 * that is in various stages of the call process called to perform some defined
 * operation.
 *
 * The SHV Method Call does the following:
 * 1. Creates response expectation (@ref rpcresponse_expect)
 * 2. Prepares for request packing (@ref rpchandler_msg_new)
 * 3. Calls the provided function with @ref CALL_S_PACK stage. The call is
 * terminated if this stage returns non-zero value (@ref rpchandler_msg_drop).
 * 4. Sends packed message (@ref rpchandler_msg_send). In case of an error the
 * provided function is called with @ref CALL_S_COMERR and call is terminated.
 * 5. Waits for response to be received (@ref rpcresponse_waitfor). On timeout
 * the call is attempted again by going back to step 2 until the attempts limit
 * is reached; in such case the provided function is called with @ref
 * CALL_S_TIMERR and call terminated.
 * 6. Calls the provided function with either @ref CALL_S_RESULT, or @ref
 * CALL_S_VOID_RESULT, or @ref CALL_S_ERROR and terminates the call.
 */

#include <shv/rpchandler_responses.h>

/*! The default number of attempts for RPC call */
#define RPCCALL_ATTEMPTS (3)
/*! The default timeout in milliseconds for a single RPC call attempt */
#define RPCCALL_TIMEOUT (60000)


/*! Stages of the SHV RPC method call. */
enum rpccall_stage {
	/*! Request message must be packed.
	 *
	 * Be aware that this stage can be called multiple times (once for every
	 * call attempt).
	 *
	 * When @ref rpccall_func_t is called with this stage the `pack` will
	 * contain the packer to be used to pack request message. `request_id` will
	 * contain the request ID that must be used for this request message (so it
	 * can be paired with response).
	 *
	 * `unpack` and `item` will be `NULL`. `ctx` is the context passed to @ref
	 * rpccall.
	 *
	 * @ref rpccall_func_t can return non-zero value to abort the call. This
	 * value is later returned from @ref rpccall. Zero means "continue".
	 */
	CALL_S_PACK,
	/*! Response was received and result value can be unpacked.
	 *
	 * Note that at this point we are not sure if message is complete or even
	 * valid and thus you should only process received result to some prepared
	 * buffer and wait with acting on it until after @ref rpccall.
	 *
	 * Be aware that this stage might be called multiple times and if you
	 * allocated any data you should be able to free it or reuse the allocated
	 * memory on subsequent iterations.
	 *
	 * When @ref rpccall_func_t is called with this stage the `unpack` and
	 * `item` will be provided. The `request_id` is provided as well but that
	 * is only for informative usages.
	 *
	 * `pack` will be `NULL`. `ctx` is the context passed to @ref rpccall.
	 */
	CALL_S_RESULT,
	/*! Response was received and caries no result value to unpack.
	 *
	 * This informs you that result was received and that it has to parameter.
	 *
	 * Dot not act on this immediately. Store info about this somewhere and
	 * perform any followup actions only after @ref rpccall.
	 *
	 * `pack`, `unpack`, and `item` will be `NULL`. `request_id` is provided for
	 * informative purposes.`ctx` is the context passed to @ref rpccall.
	 */
	CALL_S_VOID_RESULT,
	/*! Response was received and it caries an error.
	 *
	 * This the same as @ref CALL_S_RESULT with difference that instead of
	 * result value an error value should be unpacked. Please refer to the @ref
	 * CALL_S_RESULT documentation.
	 */
	CALL_S_ERROR,
	/*! Communication error was encountered when sending message.
	 *
	 * There will be no further call attempts.
	 *
	 * `pack`, `unpack`, and `item` will be `NULL`. `request_id` is provided for
	 * informative purposes.`ctx` is the context passed to @ref rpccall.
	 */
	CALL_S_COMERR,
	/*! Call timeout.
	 *
	 * Too many call attempts failed and thus call is concluded with timeout.
	 *
	 * `pack`, `unpack`, and `item` will be `NULL`. `request_id` is provided for
	 * informative purposes.`ctx` is the context passed to @ref rpccall.
	 */
	CALL_S_TIMERR,
};

/*! Functions matching this prototype will be called in various stages of the
 * call execution to pack request, handle response, and manage errors.
 *
 * The returned integer is the value to be returned from @ref rpccall with
 * notable exception for `0` in case of @ref CALL_S_PACK.
 */
typedef int (*rpccall_func_t)(enum rpccall_stage stage, cp_pack_t pack,
	int request_id, cp_unpack_t unpack, struct cpitem *item, void *ctx);

/// @cond
int _rpccall(rpchandler_t handler, rpchandler_responses_t responses,
	rpccall_func_t func, void *ctx, int attempts, int timeout)
	__attribute__((nonnull));
#define __rpccall_deft(...) _rpccall(__VA_ARGS__, RPCCALL_TIMEOUT)
#define __rpccall_def(...) __rpccall_deft(__VA_ARGS__, RPCCALL_ATTEMPTS)
#define __rpccall_noctx(...) __rpccall_def(__VA_ARGS__, NULL)
#define __rpccall_value_select(_1, _2, _3, X, ...) X
/// @endcond
/*! Call SHV RPC Method.
 *
 * This is implemented as a macro with variable number of arguments that is
 * expanded to the actual function call.
 *
 * @param HANDLER The @ref rpchandler_t object.
 * @param RESPONSES The @ref rpchandler_responses_t object that is registered as
 *   one of the stages in `HANDLER`.
 * @param FUNC Callback function @ref rpccall_func_t that is used to integrate
 *   SHV RPC Method call with caller's code. Please see the @ref rpccall_func_t
 *   and @ref rpccall_stage for more details.
 * @param CTX Is an optional argument that can be used to pass pointer to custom
 *   data to the @ref rpccall_func_t. `NULL` is used if this argument is not
 *   provided.
 * @param ATTEMPTS Is an optional argument that specifies number of attempts
 *   before call timeout is concluded. @ref RPCCALL_ATTEMPTS is used if this
 *   argument is not provided.
 * @param TIMEOUT Is an optional argument that specifies time in milliseconds
 *  before single call attempt is abandoned and new request is sent. The total
 *  timeout is at most `ATTEMPTS` times `TIMEOUT`. @ref RPCCALL_TIMEOUT is used
 *  if this argument is not provided.
 * @return Integer that is returned from `FUNC` (@ref rpccall_func_t).
 */
#define rpccall(HANDLER, RESPONSES, FUNC, ...) \
	__rpccall_value_select(__VA_ARGS_ __VA_OPT__(, ) _rpccall, __rpccall_deft, \
		__rpccall_def, __rpccall_noctx)(HANDLER, RESPONSES, FUNC, ##__VA_ARGS__)


#endif
