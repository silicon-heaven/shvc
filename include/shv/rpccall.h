/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_CALL_H
#define SHV_RPCHANDLER_CALL_H
#include <shv/rpchandler_responses.h>

/**
 * Tool to easily call methods over SHV RPC. It depends on
 * :c:type:`rpchandler_responses_t`.
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
 *
 * 1. Creates response expectation (:c:func:`rpcresponse_expect`)
 * 2. Prepares for request packing (:c:func:`rpchandler_msg_new`)
 * 3. Calls the provided function with :c:enumerator:`CALL_S_REQUEST` stage. The
 *    call is terminated if this stage returns non-zero value
 *    (:c:func:`rpchandler_msg_drop`).
 * 4. Sends packed message (:c:func:`rpchandler_msg_send`). In case of an error
 *    the provided function is called with :c:enumerator:`CALL_S_COMERR` and
 *    call is terminated.
 * 5. Waits for response to be received (:c:func:`rpcresponse_waitfor`). On
 *    timeout the call is attempted again by going back to step 2 until the
 *    attempts limit is reached; in such case the provided function is called
 *    with :c:enumerator:`CALL_S_TIMERR` and call terminated.
 * 6. Calls the provided function with :c:enumerator:`CALL_S_RESULT` to unpack
 *    value. This step is skipped if received response carried no value or if it
 *    carried an error.
 * 7. Validates the received response with :c:func:`rpchandler_msg_valid`. In
 *    case of validation failure it goes back to step 5.
 * 8. Calls the provided function with :c:enumerator:`CALL_S_DONE` and returns
 *    integer provided by the function.
 */

/** The default number of attempts for RPC call */
#define RPCCALL_ATTEMPTS (3)
/** The default timeout in milliseconds for a single RPC call attempt */
#define RPCCALL_TIMEOUT (60000)


/** Stages of the SHV RPC method call. */
enum rpccall_stage {
	/** Request message must be packed.
	 *
	 * Be aware that this stage can be called multiple times (once for every
	 * call attempt).
	 *
	 * When :c:type:`rpccall_func_t` is called with this stage the
	 * :c:var:`rpccall_ctx.pack` will contain the packer to be used to pack
	 * request message that must be request with ID
	 * :c:var:`rpccall_ctx.request_id`.
	 *
	 * :c:type:`rpccall_func_t` can return non-zero value to abort the call.
	 * This value is later returned from :c:macro:`rpccall`. Zero means
	 * "continue".
	 */
	CALL_S_REQUEST,
	/** Response was received and result value can be unpacked.
	 *
	 * Note that at this point we are not sure if message is complete or even
	 * valid and thus you should only process received result and wait with
	 * acting on it only in :c:enumerator:`CALL_S_DONE`.
	 *
	 * Be aware that this stage might be called multiple times and if you
	 * allocated any data you should be able to free it or reuse the allocated
	 * memory on subsequent iterations. Considering the sequence of the stages
	 * the best place to reset state is in :c:enumerator:`CALL_S_REQUEST`.
	 *
	 * When :c:type`rpccall_func_t` is called with this stage the
	 * ``rpccall_ctx.unpack`` and ``rpccall_ctx.item`` will be
	 * provided.
	 *
	 * The returned value from :c:type:`rpccall_func_t` has no effect.
	 */
	CALL_S_RESULT,
	/** The method call is done.
	 *
	 * Here you can act upon anything you received in
	 * :c:enumerator:`CALL_S_RESULT`.
	 *
	 * You should check ``rpccall_ctx.errnum`` to see if there was an error
	 * detected or not.
	 *
	 * The value returned from :c:type:`rpccall_func_t` is returned by
	 * :c:macro:`rpccall`.
	 */
	CALL_S_DONE,
	/** Communication error was encountered when sending message.
	 *
	 * There will be no further call attempts.
	 *
	 * The value returned from :c:type:`rpccall_func_t` is returned by
	 * :c:macro:`rpccall`.
	 */
	CALL_S_COMERR,
	/** Call timeout.
	 *
	 * Too many call attempts failed and thus call is concluded with timeout.
	 *
	 * The value returned from :c:type:`rpccall_func_t` is returned by
	 * :c:macro:`rpccall`.
	 */
	CALL_S_TIMERR,
};

/** Context passsed to the :c:type:`rpccall_func_t`. */
struct rpccall_ctx {
	/** Cookie passed to the :c:macro:`rpccall`.
	 *
	 * You can even modify this because this structure is kept for duration of
	 * the :c:macro:`rpccall` execution.
	 */
	void *cookie;
	/** Local cookie where you can save anything you need in between the calls.
	 *
	 * Make sure that anything you allocate you will also free in
	 * :c:enumerator:`CALL_S_DONE`, :c:enumerator:`CALL_S_COMERR`, and
	 * :c:enumerator:`CALL_S_TIMERR` stages.
	 */
	void *lcookie;
	/** The request ID for this RPC call. */
	const int request_id;
	/** */
	union {
		/** Packer you should use to pack request message.
		 *
		 * This must be used only in :c:enumerator:`CALL_S_REQUEST` stage!
		 */
		cp_pack_t pack;
		/** Values used only in :c:enumerator:`CALL_S_RESULT`. */
		struct {
			/** Unpacker you should use to unpack result. */
			cp_unpack_t unpack;
			/** Item used to unpack result message. */
			struct cpitem *item;
		};
		/** Values used only in :c:enumerator:`CALL_S_DONE`. */
		struct {
			/** RPC Error number.
			 *
			 * This is :c:macro:`RPCERR_NO_ERROR` in case no error.
			 */
			rpcerrno_t errnum;
			/** RPC Error message.
			 *
			 * This can be ``NULL`` if there was no error or when error had no
			 * message.
			 */
			char *errmsg;
		};
	};
};

/** Functions matching this prototype will be called in various stages of the
 * call execution to pack request, handle response, and manage errors.
 *
 * The returned integer is the value to be returned from :c:macro:`rpccall` with
 * notable exception for ``0`` in case of :c:enumerator:`CALL_S_REQUEST`.
 */
typedef int (*rpccall_func_t)(enum rpccall_stage stage, struct rpccall_ctx *ctx);

[[gnu::nonnull(1, 2, 3)]]
int _rpccall(rpchandler_t handler, rpchandler_responses_t responses,
	rpccall_func_t func, void *ctx, int attempts, int timeout);
#define __rpccall_deft(...) _rpccall(__VA_ARGS__, RPCCALL_TIMEOUT)
#define __rpccall_def(...) __rpccall_deft(__VA_ARGS__, RPCCALL_ATTEMPTS)
#define __rpccall_noctx(...) __rpccall_def(__VA_ARGS__, NULL)
#define __rpccall_value_select(_1, _2, _3, X, ...) X
/** Call SHV RPC Method.
 *
 * This is implemented as a macro with variable number of arguments that is
 * expanded to the actual function call.
 *
 * :param HANDLER: The :c:type:`rpchandler_t` object.
 * :param RESPONSES: The :c:type:`rpchandler_responses_t` object that is
 *   registered as one of the stages in ``HANDLER``.
 * :param FUNC: Callback function :c:type:`rpccall_func_t` that is used to
 *   integrate SHV RPC Method call with caller's code. Please see the
 *   :c:type:`rpccall_func_t` and :c:enum:`rpccall_stage` for more details.
 * :param CTX: can be used to pass pointer to custom data to the
 *   :c:type:`rpccall_func_t`. ``NULL`` is used if this argument is not
 *   provided.
 * :param ATTEMPTS: specifies number of attempts before call timeout is
 *   concluded. :c:macro:`RPCCALL_ATTEMPTS` is used if this argument is not
 *   provided.
 * :param TIMEOUT: specifies time in milliseconds before single call attempt is
 *   abandoned and new request is sent. The total timeout is at most
 *   `ATTEMPTS` times `TIMEOUT`. :c:macro:`RPCCALL_TIMEOUT` is used if this
 *   argument is not provided.
 * :return: Integer that is returned from ``FUNC`` (:c:type:`rpccall_func_t`).
 */
#define rpccall(HANDLER, RESPONSES, FUNC, ...) \
	__rpccall_value_select(__VA_ARGS_ __VA_OPT__(, ) _rpccall, __rpccall_deft, \
		__rpccall_def, __rpccall_noctx)(HANDLER, RESPONSES, FUNC, ##__VA_ARGS__)

#endif
