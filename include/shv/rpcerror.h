/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCERROR_H
#define SHV_RPCERROR_H
/*! @file
 * Common errors used in the SHV RPC.
 */

#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>

/*! Type alias for RPC error codes. Predefined errors are provided as macros
 * prefixed with `RPCERR_`. */
typedef unsigned rpcerrno_t;

/*! No error that is mostly internally used to identify no errors. */
#define RPCERR_NO_ERROR (0)
/*! Sent request is invalid in its format. */
#define RPCERR_INVALID_REQUEST (1)
/*! Method is can't be found or you do not have access to it. */
#define RPCERR_METHOD_NOT_FOUND (2)
/*! Request received but with invalid parameters. */
#define RPCERR_INVALID_PARAMS (3)
/*! Request can't be processes due to the internal error. */
#define RPCERR_INTERNAL_ERR (4)
/*! Message content can't be parsed correctly. */
#define RPCERR_PARSE_ERR (5)
/*! Request timed out without response. */
#define RPCERR_METHOD_CALL_TIMEOUT (6)
/*! Request got canceled. */
#define RPCERR_METHOD_CALL_CANCELLED (7)
/*! Request was received successfully but issue was encountered when it was
 * being acted upon it.
 */
#define RPCERR_METHOD_CALL_EXCEPTION (8)
/*! Generic unknown error assigned when we are not aware what happened. */
#define RPCERR_UNKNOWN (9)
/*! Method call without previous successful login. */
#define RPCERR_LOGIN_REQUIRED (10)
/*! Method call requires UserID to be present in the request message. */
#define RPCERR_USER_ID_REQUIRED (11)
/*! Can be used if method is valid but not implemented for whatever reason. */
#define RPCERR_NOT_IMPLEMENTED (12)
/*! The first user defined error code. The lower values are reserved for the
 * SHV defined errors.
 */
#define RPCERR_USER_CODE (32)

/*! Keys used in RPC error *IMap*, that follows @ref RPCMSG_KEY_ERROR. */
enum rpcmsg_error_key {
	/*! Must be followed by *Int* value specifying the error code. */
	RPCMSG_ERR_KEY_CODE = 1,
	/*! Message describing cause of the error. */
	RPCMSG_ERR_KEY_MESSAGE,
};

/*! Unpack RPC error.
 *
 * This must be called right after @ref rpcmsg_head_unpack to correctly unpack
 * error.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the @ref cp_unpack calls and was used in the last
 *   one.
 * @param errno: Pointer to the variable where error number is placed. Can be
 *   `NULL` if you are not interested in the error code.
 * @param errmsg: Pointer to the variable where error message is placed. Error
 *   message is copied to the memory allocated using malloc and thus do not
 *   forget to free it. You can pass `NULL` if you are not interested in the
 *   error message.
 * @returns `true` if error was unpacked correctly, `false` otherwise.
 */
bool rpcerror_unpack(cp_unpack_t unpack, struct cpitem *item, rpcerrno_t *errno,
	char **errmsg) __attribute__((nonnull(1, 2)));

#endif
