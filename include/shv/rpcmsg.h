/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCMSG_H
#define SHV_RPCMSG_H
/*! @file
 * Functions to pack and unpack RPC messages.
 */

#include <stdbool.h>
#include <stdarg.h>
#include <obstack.h>
#include <sys/types.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <shv/rpcaccess.h>
#include <shv/rpcerror.h>

/*! Keys used in RPC message meta. */
enum rpcmsg_tags {
	/*! Identifier of the message type. */
	RPCMSG_TAG_META_TYPE_ID = 1,
	/*! Identifier for the message namespace. */
	RPCMSG_TAG_META_TYPE_NAMESPACE_ID = 2,
	/*! Request identifier used for request and response messages. */
	RPCMSG_TAG_REQUEST_ID = 8,
	/*! Path in the SHV tree to the specific node. */
	RPCMSG_TAG_SHV_PATH = 9,
	/*! Method name associated with node on SHV path. */
	RPCMSG_TAG_METHOD = 10,
	/*! Signal name associated with source method. */
	RPCMSG_TAG_SIGNAL = 10,
	/*! Identifiers for callers. */
	RPCMSG_TAG_CALLER_IDS = 11,
	/*! Access associated with this message (prefer to use access level
	 * instead).
	 */
	RPCMSG_TAG_ACCESS = 14,
	/*! ID of the user calling RPC method. */
	RPCMSG_TAG_USER_ID = 16,
	/*! Access level assigned to this message */
	RPCMSG_TAG_ACCESS_LEVEL = 17,
	/*! Name of the method signal is associated with. */
	RPCMSG_TAG_SOURCE = 19,
	/*! Used for the signals to signal that this is repeat of older signal. */
	RPCMSG_TAG_REPEAT = 20,
};

/*! Keys used in RPC top level *IMap*. */
enum rpcmsg_keys {
	/*! Parameter used for requests and signals. */
	RPCMSG_KEY_PARAM = 1,
	/*! Result used for successful responses. */
	RPCMSG_KEY_RESULT,
	/*! Result used for failed responses. */
	RPCMSG_KEY_ERROR,
};


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

/*! Parsed content of RPC Message's Meta.
 *
 * Parsing is performed with @ref rpcmsg_head_unpack.
 */
struct rpcmsg_meta {
	/*! Type of the message. */
	enum rpcmsg_type type;

	/*! Request ID if message is request or response.
	 *
	 * Valid for @ref RPCMSG_T_REQUEST, @ref RPCMSG_T_RESPONSE, and @ref
	 * RPCMSG_T_ERROR.
	 */
	int64_t request_id;
	/*! Access level for request and signal messages.
	 *
	 * Valid for @ref RPCMSG_T_REQUEST and @ref RPCMSG_T_SIGNAL.
	 */
	rpcaccess_t access;
	/*! Granted access level without access level.
	 *
	 * This is pre-SHV 3.0 access level control. It is a comma delimited
	 * sequence of tokens with tokens matching the access control levels
	 * removed.
	 *
	 * Might be valid for @ref RPCMSG_T_REQUEST but you should always expect it
	 * being `NULL` as well.
	 */
	char *access_granted;

	/*! Path to the SHV node.
	 *
	 * Valid for @ref RPCMSG_T_REQUEST and @ref RPCMSG_T_SIGNAL.
	 */
	char *path;
	// @cond
	union {
		// @endcond
		/*! Method name if message is request.
		 *
		 * Valid for @ref RPCMSG_T_REQUEST.
		 */
		char *method;
		/*! Signal name if message is signal.
		 *
		 * Valid for @ref RPCMSG_T_SIGNAL.
		 */
		char *signal;
		// @cond
	};
	// @endcond
	/*! Method name signal is associated with.
	 *
	 * Valid for @ref RPCMSG_T_SIGNAL.
	 */
	char *source;
	/*! Informs that this is repeated signal for signal messages.
	 *
	 * Valid for @ref RPCMSG_T_SIGNAL.
	 */
	bool repeat;
	/*! User's ID. Is `NULL` in case it wasn't provided.
	 *
	 * Valid for @ref RPCMSG_T_REQUEST and @ref RPCMSG_T_SIGNAL.
	 */
	char *user_id;

	/*! Client IDs if message is request or response.
	 *
	 * This is array of integers where size is in `cids_cnt`. This can be `NULL`
	 * if `cids_cnt == 0`.
	 *
	 * Valid for @ref RPCMSG_T_REQUEST, @ref RPCMSG_T_RESPONSE, and @ref
	 * RPCMSG_T_ERROR.
	 */
	long long *cids;
	/*! Length of the `cids` array. */
	size_t cids_cnt;

	/*! Additional extra fields found in meta as linked list. */
	struct rpcmsg_meta_extra {
		/*! Integer key this value had in meta. */
		int key;
		/*! Pointer to the ChainPack data. */
		void *ptr;
		/*! Number of valid bytes in `ptr` */
		size_t siz;
		/*! Next meta or `NULL` if this is the last one. */
		struct rpcmsg_meta_extra *next;
	}
		/*! Pointer to the first extra meta or `NULL`. */
		*extra;
};

/*! Limits imposed on @ref rpcmsg_meta fields.
 *
 * This is used by @ref rpcmsg_head_unpack to limit size of dynamically
 * allocated buffers.
 *
 * The dynamic allocation can be constrained or unconstrained. The default
 * behavior if you pass no limits to the @ref rpcmsg_head_unpack is to use as
 * much space as required. Not only that this results in possible crash if
 * message with too big fields is received, it is primarily not necessary. The
 * devices commonly know how long paths and methods they support and anything
 * longer is invalid anyway and thus in most cases it servers no purpose to
 * store actually the whole value.
 */
struct rpcmsg_meta_limits {
	/*! Limit on the @ref rpcmsg_meta.path size.
	 *
	 * You want to set this to one or two bytes longer than your longest
	 * supported path. It is to detect that this is too long path (because that
	 * won't be signaled to you in any other way).
	 *
	 * You can it to `-1` for unconstrained allocation.
	 */
	ssize_t path;
	/*! Limit on @ref rpcmsg_meta.method, @ref rpcmsg_meta.source, and @ref
	 * rpcmsg_meta.signal size.
	 *
	 * The same suggestion applies here as for @ref rpcmsg_meta_limits.path.
	 */
	ssize_t name;
	/*! Limit on @ref rpcmsg_meta.user_id.
	 *
	 * Depending on your use case for the User's ID you want to set this to
	 * size you are willing to handle (such as store to the logs). The head of
	 * the User's ID is kept while tail is dropped. Note that the head commonly
	 * contains the most important information because that is identifier of the
	 * top level user.
	 */
	ssize_t user_id;
	/*! If extra parameters should be preserved or no.
	 *
	 * The default is that they are dropped (`false`). In most cases  they are
	 * not needed. This is only essential for Broker that should preserve all
	 * parameters not just those he knows.
	 */
	bool extra;
};

/*! Default limits used by @ref rpcmsg_head_unpack. */
extern const struct rpcmsg_meta_limits rpcmsg_meta_limits_default;

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
 *   the data are stored on obstack.
 * @param limits: Optional pointer to the limits imposed on the meta attributes.
 *   @ref rpcmsg_meta_limits_default is used if `NULL` is passed.
 * @param obstack: Pointer to the obstack used to allocate space for received
 *   data.
 * @returns `false` in case unpack reports error, if meta contains invalid value
 *   for supported key. In other cases `true` is returned. In short this return
 *   primarily signals if it is possible to generate response to this message
 *   for request messages and if it is possible to parameters, response or error
 *   for other message types.
 */
bool rpcmsg_head_unpack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta *meta, const struct rpcmsg_meta_limits *limits,
	struct obstack *obstack) __attribute__((nonnull(1, 2, 3, 5)));

/*! Query if message has parameter to unpack.
 *
 * Use this right after @ref rpcmsg_head_unpack to check if parameter can be
 * unpacked.
 *
 * Technically this only checks if currently unpacked item is @ref
 * CPITEM_CONTAINER_END. The message that doesn't have argument will end
 * immediately and thus subsequent unpacks will most likely result into an
 * error.
 *
 * @param item: Item used for the @ref rpcmsg_head_unpack call.
 * @returns `true` if there is parameter to unpack and `false` if there are not.
 */
__attribute__((nonnull)) static inline bool rpcmsg_has_param(struct cpitem *item) {
	return item->type != CPITEM_CONTAINER_END;
}

/*! Get new request ID.
 *
 * This is program wide global variable access protected to be thread safe. The
 * request ID goes from 4 to 63 and then it wraps. The 1 - 3 is reserved for
 * login and ping operations. The upper limit of 63 comes from ChainPack where
 * it is the largest value still packed as a single integer.
 *
 * @returns Request ID.
 */
int rpcmsg_request_id(void);

/*! Pack request message meta and open IMap. The followup packed data are
 * parameters provided to the method call request. The message needs to be
 * terminated with container end (@ref cp_pack_container_end).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node the method we want to request is associated
 *   with.
 * @param method: name of the method we request to call.
 * @param uid: User's  ID to be added to the request. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * @param rid: request identifier. Thanks to this number you can associate
 *   response with requests.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_request(cp_pack_t pack, const char *path, const char *method,
	const char *uid, int rid) __attribute__((nonnull(1, 2, 3)));

/*! Pack request message with no parameters.
 *
 * This provides an easy way to just pack message without arguments. It packs
 * the same head as @ref rpcmsg_pack_request plus additional container end to
 * complete the message. Such message can be immediately send.
 *
 * By using this over @ref rpcmsg_pack_request you will also save two bytes in
 * the ChainPack message.
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node the method we want to request is associated
 * with.
 * @param method: name of the method we request to call.
 * @param uid: User's  ID to be added to the request. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * @param rid: request identifier. Thanks to this number you can associate
 * response with requests.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_request_void(cp_pack_t pack, const char *path, const char *method,
	const char *uid, int rid) __attribute__((nonnull(1, 2, 3)));

/*! Pack signal message meta and open IMap. The followup packed data are
 * signaled values. The message needs to be terminated with container end
 * (@ref cp_pack_container_end).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node method is associated with.
 * @param source: name of the method the signal is associated with.
 * @param signal: name of the signal.
 * @param uid: User's ID to be added to the signal. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * @param access: The access level for this signal. This is used to filter
 *   access  to the signals and in general should be consistent with access
 *   level of the method this signal is associated with.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_signal(cp_pack_t pack, const char *path, const char *source,
	const char *signal, const char *uid, rpcaccess_t access)
	__attribute__((nonnull(1, 2, 3, 4)));

/*! Pack signal message with no value.
 *
 * This provides an easy way to just pack signal without value. It packs the
 * same head as @ref rpcmsg_pack_signal plus additional container end to
 * complete the message. Such message can be immediately send.
 *
 * By using this over @ref rpcmsg_pack_signal you will also save two bytes in
 * the ChainPack message.
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node method is associated with.
 * @param source: name of the method the signal is associated with.
 * @param signal: name of the signal.
 * @param uid: User's ID to be added to the signal. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * @param access: The access level for this signal. This is used to filter
 *   access  to the signals and in general should be consistent with access
 *   level of the method this signal is associated with.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_signal_void(cp_pack_t pack, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access)
	__attribute__((nonnull(1, 2, 3, 4)));

/*! Pack response message meta based on the provided meta.
 *
 * @param pack: Packer the meta should be written to.
 * @param meta: Meta with info for the received request.
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
 * @param pack: Packer the meta should be written to.
 * @param meta: Meta with info for the received request.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_response_void(cp_pack_t pack, const struct rpcmsg_meta *meta)
	__attribute__((nonnull));

/*! Pack error response message based on the provided meta.
 *
 * This is helper to pack the whole error message at once. After this you can
 * directly send the message.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @param errno: Error code to be reported as response to the request.
 * @param msg: Optional message describing the error details.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_error(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errno, const char *msg) __attribute__((nonnull(1, 2)));

/*! Pack error response message based on the provided meta.
 *
 * This is variant of @ref rpcmsg_pack_error that allows you to use printf
 * format string to generate the error message. This function packs complete
 * message and thus you can immediately send it.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @param errno: Error code to be reported as response to the request.
 * @param fmt: Format string used to generate the error message.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_ferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errno, const char *fmt, ...) __attribute__((nonnull(1, 2, 4)));

/*! Pack error response message based on the provided meta.
 *
 * This is variant of @ref rpcmsg_pack_error that allows you to use printf
 * format string to generate the error message. This function packs complete
 * message and thus you can immediately send it.
 *
 * @param pack: Pack context the meta should be written to.
 * @param meta: Meta with info for previously received request.
 * @param errno: Error code to be reported as response to the request.
 * @param fmt: Format string used to generate the error message.
 * @param args: Variable list of arguments to be used with **fmt**.
 * @returns Boolean signaling the pack success or failure.
 */
bool rpcmsg_pack_vferror(cp_pack_t pack, const struct rpcmsg_meta *meta,
	rpcerrno_t errno, const char *fmt, va_list args)
	__attribute__((nonnull(1, 2, 4)));

#endif
