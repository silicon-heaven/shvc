/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_CALL_H
#define SHV_RPCHANDLER_CALL_H
/*! @file
 * Macro only utility to call methods with RPC Handler and receive resposne with
 * RPC Handler Responses.
 */

#include <shv/rpchandler_responses.h>

/*! The default number of attempts for RPC call */
#define RPCCALL_ATTEMPTS (3)
/*! The default timeout in seconds for a single RPC call attempt */
#define RPCCALL_TIMEOUT (300)


enum rpccall_stage {
	CALL_S_PACK,
	CALL_S_RESULT,
	CALL_S_VOID_RESULT,
	CALL_S_ERROR,
	CALL_S_COMERR,
	CALL_S_TIMERR,
};

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
/*!
 */
#define rpccall(HANDLER, RESPONSES, FUNC, ...) \
	__rpccall_value_select(__VA_ARGS_ __VA_OPT__(, ) _rpccall, __rpccall_deft, \
		__rpccall_def, \
		__rpccall_noctx)(HANDLER, RESPONSES, FUNC __VA_OPT__(, ) __VA_ARGS__)


#endif
