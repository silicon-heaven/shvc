/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCMSG_H
#define SHV_RPCMSG_H
/*! @file
 * Functions to pack and unpack RPC messages.
 */

#include <stdbool.h>
#include <stdarg.h>
#include <sys/types.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <obstack.h>

/*! Keys used in RPC message meta. */
enum rpcmsg_tags {
	/*! Identifier of the message type. */
	RPCMSG_TAG_META_TYPE_ID = 1,
	/*! Identifier for the message namespace. */
	RPCMSG_TAG_META_TYPE_NAMESPACE_ID,
	/*! Request identifier used for request and response messages. */
	RPCMSG_TAG_REQUEST_ID = 8,
	/*! Path in the SHV tree to the specific node. */
	RPCMSG_TAG_SHV_PATH,
	/*! Method name associated with node on SHV path. */
	RPCMSG_TAG_METHOD,
	/*! Identifiers for callers. */
	RPCMSG_TAG_CALLER_IDS,
	/*! Identifiers for the reverse callers. */
	RPCMSG_TAG_REV_CALLER_IDS,
	/*! Access level associated with this message. */
	RPCMSG_TAG_ACCESS,
};

/*! Keys used in RPC top level *IMap*. */
enum rpcmsg_keys {
	/*! Parameter used for requests and signals. */
	RPCMSG_KEY_PARAMS = 1,
	/*! Result used for successful responses. */
	RPCMSG_KEY_RESULT,
	/*! Result used for failed responses. */
	RPCMSG_KEY_ERROR,
};

/*! Keys used in RPC error *IMap*, that follows @ref RPCMSG_KEY_ERROR. */
enum rpcmsg_error_key {
	/*! Must be followed by *Int* value specifying the error code (see @ref
	 * rpcmsg_error).
	 */
	RPCMSG_ERR_KEY_CODE = 1,
	/*! Message describing cause of the error. */
	RPCMSG_ERR_KEY_MESSAGE,
};

/*! Pack request message meta and open imap. The followup packed data are
 * parameters provided to the method call request. The message needs to be
 * terminated with container end (@ref cp_pack_container_end).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node the method we want to request is associated
 *   with.
 * @param method: name of the method we request to call.
 * @param rid: request identifier. Thanks to this number you can associate
 *   response with requests.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_request(cp_pack_t pack, const char *path, const char *method,
	int rid) __attribute__((nonnull(1, 3)));

/*! Pack request message with no parameters.
 *
 * This provides an easy way to just pack message without arguments. It packs
 * the same head as @ref rpcmsg_pack_request plus additional container end to
 * complete the message. Such message can be immediately send.
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node the method we want to request is associated
 * with.
 * @param method: name of the method we request to call.
 * @param rid: request identifier. Thanks to this number you can associate
 * response with requests.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_request_void(cp_pack_t pack, const char *path,
	const char *method, int rid) __attribute__((nonnull(1, 3)));

/*! Pack signal message meta and open imap. The followup packed data are
 * signaled values. The message needs to be terminated with container end
 * (@ref cp_pack_container_end).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node method is associated with.
 * @param method: name of the method signal is raised for.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *method)
	__attribute__((nonnull(1, 3)));

/*! Pack value change signal message meta and open imap. The followup packed
 * data are signaled values. The message needs to be terminated with container
 * end (@ref cp_pack_container_end).
 *
 * This is actually just a convenient way to call @ref rpcmsg_pack_signal for
 * "chng" methods.
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path the value change signal is associated with.
 * @returns Boolean signaling the pack success or failure.
 */
__attribute__((nonnull(1))) static inline bool rpcmsg_pack_chng(
	cp_pack_t pack, const char *path) {
	return rpcmsg_pack_signal(pack, path, "chng");
}


/*! Access levels supported by SHVC. */
enum rpcmsg_access {
	RPCMSG_ACC_INVALID,
	RPCMSG_ACC_BROWSE,
	RPCMSG_ACC_READ,
	RPCMSG_ACC_WRITE,
	RPCMSG_ACC_COMMAND,
	RPCMSG_ACC_CONFIG,
	RPCMSG_ACC_SERVICE,
	RPCMSG_ACC_SUPER_SERVICE,
	RPCMSG_ACC_DEVEL,
	RPCMSG_ACC_ADMIN,
};

/*! Convert access level to its string representation.
 *
 * @param access: Access level to be converted.
 * @returns Pointer to string constant representing the access level.
 */
const char *rpcmsg_access_str(enum rpcmsg_access access);

/*! Extract access level from string.
 *
 * @param str: String with access specifier.
 * @returns Access level.
 */
enum rpcmsg_access rpcmsg_access_extract(const char *str);

/*! Unpack access string and extract access level from it.
 *
 * You can detect unpack error by checking `item->type != CP_ITEM_INVALID`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the @ref cp_unpack calls and was used in the last
 *   one.
 * @returns Access level.
 */
enum rpcmsg_access rpcmsg_access_unpack(cp_unpack_t unpack, struct cpitem *item);



/*! Identification of the message. */
enum rpcmsg_type {
	/*! Message is not know or has invalid meta */
	RPCMSG_T_INVALID,
	/*! Message is request (`request_id` is valid alongside with `method`). */
	RPCMSG_T_REQUEST,
	/*! Message is response (`request_id` is valid). */
	RPCMSG_T_RESPONSE,
	/*! Message is response with error attached (`request_id` is valid). */
	RPCMSG_T_ERROR,
	/*! Message is signal (`method` is valid). */
	RPCMSG_T_SIGNAL,
};

/*! Pointer to data (`ptr`) with `siz` number of valid bytes. */
struct rpcmsg_ptr {
	/*! Pointer to the data. */
	void *ptr;
	/*! Number of valid bytes in @ref ptr */
	size_t siz;
};

/*! Parsed  content of RPC Message Meta.
 *
 * Parsing is performed with @ref rpcmsg_head_unpack.
 */
struct rpcmsg_meta {
	/*! Type of the message. */
	enum rpcmsg_type type;
	/*! Request ID if message is request or response. */
	int64_t request_id;
	/*! Path to the SHV node. Can be `NULL`. */
	char *path;
	/*! Method name if message is request or signal. */
	char *method;
	/*! Granted access level for request messages. */
	enum rpcmsg_access access_grant;
	/*! Client IDs in Chainpack if message is request. */
	struct rpcmsg_ptr cids;

	/*! Additional extra fields found in meta as linked list. */
	struct rpcmsg_meta_extra {
		/*! Integer key this value had in meta. */
		int key;
		/*! Pointer and size of the value. */
		struct rpcmsg_ptr ptr;
		/*! Next meta or `NULL` if this is the last one. */
		struct rpcmsg_meta_extra *next;
	}
		/*! Pointer to the first extra meta or `NULL`. */
		* extra;
};

/*! Limits imposed on @ref rpcmsg_meta fields.
 *
 * This is used by @ref rpcmsg_head_unpack in two modes: It provides size of the
 * buffers pointed in @ref rpcmsg_meta; It provides limit on dynamically
 * allocated buffers.
 *
 * The dynamic allocation can be constrained or unconstrained. The default
 * behavior if you pass no limits to the @ref rpcmsg_head_unpack is to use as
 * much space as required. Not only that this results in possible crash if
 * message with too big fields is received, it is primarily not necessary. The
 * devices commonly know how long paths and methods they support and anything
 * longer is invalid anyway and thus in most cases it servers no purpose to
 * store actually the whole value.
 *
 * To allow constrained allocation for some of the fields but not for other you
 * can use `-1` as limit which informs @ref rpcmsg_head_unpack that there is no
 * limit for that field.
 */
struct rpcmsg_meta_limits {
	/*! Limit on the `path` size.
	 *
	 * You want to set this to one or two bytes longer than your longest
	 * supported path. It is to detect that this is too long path (because that
	 * won't be signaled to you in any other way).
	 */
	ssize_t path;
	/*! Limit on `method` size.
	 *
	 * The same suggestion applies here as for `path`.
	 */
	ssize_t method;
	/*! Limit on `cids.ptr` buffer.
	 *
	 * It is highly suggested to set this to `-1` or some high number if you use
	 * obstack. That is because messages without client IDs can't be responded
	 * due to the not arriving to the source without this reverse path.
	 *
	 * In case you do not use obstack then set buffer size of `cids.ptr` as
	 * usual.
	 */
	ssize_t cids;
	/*! If extra parameters should be preserved or no.
	 *
	 * The default is that they are dropped (`false`). In most cases that is
	 * what they are not needed. This is only essential for Broker that should
	 * preserve all parameters not just those he knows.
	 */
	bool extra;
};

/*! Unpack meta and openning of iMap of the RPC message.
 *
 * The unpacking can be performed in two different modes. You can either provide
 * obstack and in such case data would be pushed to it (highly suggested). But
 * if you know all limits upfront you can also just pass `NULL` to **obstack**
 * and in such case we expect that provided **meta** is already seeded with
 * buffers of sizes specified in **limits**.
 *
 * WARNING: The limits when **obstack** is provided are not enforced right now.
 * This is missing implementation not intended state!
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the @ref cp_unpack calls and was used in the last
 *   one.
 * @param meta: Pointer to the structure where unpacked data will be placed. In
 *   case of integers the value is directly stored. For strings and byte arrays
 *   the data are stored on obstack, if provided, and meta contains pointers to
 *   this data. If you do not provide **obstack** then pointers in the meta need
 *   to point to valid buffers.
 * @param limits: Optional pointer to the limits imposed on the meta attributes.
 *   This is used for two purposes: It specifies size of the buffers if you do
 *   not provide **obstack** It limits maximal length for data copied to the
 *   obstack. Please see @ref rpcmsg_meta_limits for explanation.
 * @param obstack: Pointer to the obstack used to allocate space for received
 *   strings and byte arrays. You can pass `NULL` but in such case you need to
 *   provide your own buffers in **meta**.
 * @returns `false` in case unpack reports error, if meta contains invalid value
 *   for supported key, or when there is not enough space to store caller IDs.
 *   In other cases `true` is returned. In short this return primarily signals
 *   if it is possible to generate response to this message for request messages
 *   and if it is possible to parameters, response or error for other message
 *   types.
 */
bool rpcmsg_head_unpack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta *meta, struct rpcmsg_meta_limits *limits,
	struct obstack *obstack);


/*! Pack response message meta based on the provided meta.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_response(cp_pack_t pack, const struct rpcmsg_meta *meta)
	__attribute__((nonnull));

/*! Pack response message based on the provided meta without any value.
 *
 * This is helper to pack the complete message at once when response is *void*
 * (only confirm without any real value returned). After this you can directly
 * send the message.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_response_void(cp_pack_t pack, const struct rpcmsg_meta *meta)
	__attribute__((nonnull));

/*! Known error codes defined in SHV. */
enum rpcmsg_error {
	/*! No error that is mostly internally used to identify no errors. */
	RPCMSG_E_NO_ERROR,
	/*! Sent request is invalid in its format. */
	RPCMSG_E_INVALID_REQUEST,
	/*! Method is can't be found or you do not have access to it. */
	RPCMSG_E_METHOD_NOT_FOUND,
	/*! Request received but with invalid parameters. */
	RPCMSG_E_INVALID_PARAMS,
	/*! Request can't be processes due to the internal error. */
	RPCMSG_E_INTERNAL_ERR,
	/*! Message content can't be parsed correctly. */
	RPCMSG_E_PARSE_ERR,
	/*! Request timed out without response. */
	RPCMSG_E_METHOD_CALL_TIMEOUT,
	/*! Request got canceled. */
	RPCMSG_E_METHOD_CALL_CANCELLED,
	/*! Request was received successfully but issue was encountered when it was
	 * being acted upon it.
	 */
	RPCMSG_E_METHOD_CALL_EXCEPTION,
	/*! Generic unknown error assigned when we are not aware what happened. */
	RPCMSG_E_UNKNOWN,
	/*! The first user defined error code. The lower values are reserved for the
	 * SHV defined errors.
	 */
	RPCMSG_E_USER_CODE = 32,
};

/*! Pack error response message based on the provided meta.
 *
 * This is helper to pack the whole error message at once. After this you can
 * direcly send the message.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @param error: Error code to be reported as response to the request.
 * @param msg: Optional message describing the error details.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_error(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *msg) __attribute__((nonnull(1, 2)));

/*! Pack error response message based on the provided meta.
 *
 * This is variant of @ref rpcmsg_pack_error that allows you to use printf
 * format string to generate the error message. This function packs complete
 * message and thus you can immediately send it.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @param error: Error code to be reported as response to the request.
 * @param fmt: Format string used to generate the error message.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_ferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, ...) __attribute__((nonnull(1, 2)));

/*! Pack error response message based on the provided meta.
 *
 * This is variant of @ref rpcmsg_pack_error that allows you to use printf
 * format string to generate the error message. This function packs complete
 * message and thus you can immediately send it.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @param error: Error code to be reported as response to the request.
 * @param fmt: Format string used to generate the error message.
 * @param args: Variable list of arguments to be used with **fmt**.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_vferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, va_list args)
	__attribute__((nonnull(1, 2)));

/*! Unpack error.
 *
 * This must be called right after @ref rpcmsg_head_unpack to correctly unpack
 * error.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the @ref cp_unpack calls and was used in the last
 *   one.
 * @param errnum: Pointer to the variable where error number is placed. Can be
 *   `NULL` if you are not interested in the error code.
 * @param errmsg: Pointer to the variable where error message is placed. Error
 *   message is copied to the memory allocated using malloc and thus do not
 *   forget to free it. You can pass `NULL` if you are not interested in the
 *   error message.
 * @returns `true` if error was unpacked correctly, `false` otherwise.
 */
bool rpcmsg_unpack_error(cp_unpack_t unpack, struct cpitem *item,
	enum rpcmsg_error *errnum, char **errmsg) __attribute__((nonnull(1, 2)));

#endif
