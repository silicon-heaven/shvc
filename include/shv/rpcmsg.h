/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCMSG_H
#define SHV_RPCMSG_H

#include <stdbool.h>
#include <stdarg.h>
#include <sys/types.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <obstack.h>

enum rpcmsg_tags {
	RPCMSG_TAG_UNKNOWN = 0,
	RPCMSG_TAG_META_TYPE_ID,
	RPCMSG_TAG_META_TYPE_NAMESPACE_ID,
	RPCMSG_TAG_REQUEST_ID = 8,
	RPCMSG_TAG_SHV_PATH,
	RPCMSG_TAG_METHOD,
	RPCMSG_TAG_CALLER_IDS,
	RPCMSG_TAG_REV_CALLER_IDS,
	RPCMSG_TAG_ACCESS_GRANT,
	RPCMSG_TAG_USER_ID,
};

enum rpcmsg_keys {
	RPCMSG_KEY_PARAMS = 1,
	RPCMSG_KEY_RESULT,
	RPCMSG_KEY_ERROR,
};

enum rpcmsg_error_key {
	RPCMSG_ERR_KEY_CODE = 1,
	RPCMSG_ERR_KEY_MESSAGE,
};

/*! Pack request message meta and open imap. The followup packed data are
 * parameters provided to the method call request. The message needs to be
 * terminated with container end (`chainpack_pack_container_end`).
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
 * the same head as `rpcmsg_pack_request` plus additional `CONTAINER_END` to
 * complete the message. Such message can be immediately sent.
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
 * (`chainpack_pack_container_end`).
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
 * end (`chainpack_pack_container_end`).
 *
 * This is actually just a convenient way to call `rpcmsg_pack_signal` for
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
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
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
	void *ptr;
	size_t siz;
};

/*! Parsed  content of RPC Message Meta.
 *
 * Parsing is performed with `rpcmsg_meta_unpack`.
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

	struct rpcmsg_meta_extra {
		int key;
		struct rpcmsg_ptr ptr;
		struct rpcmsg_meta_extra *next;
	} * extra;
};

/*! Limits imposed on `struct rpcmsg_meta` fields.
 *
 * This is used by `rpcmsg_meta_unpack` in two modes: It provides size of the
 * buffers pointed in `struct rpcmsg_meta`; It provides limit on dynamically
 * allocated buffers.
 *
 * The dynamic allocation can be constrained or unconstrained. The default
 * behavior if you pass no limits to the `rpcmsg_meta_unpack` is to use as much
 * space as required. Not only that this results in possible crash if message
 * with too big fields is received, it is primarily not necessary. The devices
 * commonly know how long paths and methods they support and anything longer is
 * invalid anyway and thus in most cases it servers no purpose to store actually
 * the whole value.
 *
 * To allow constrained allocation for some of the fields but not for other you
 * can use `-1` as limit which informs `rpcmsg_meta_unpack` that there is no
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
 * if you know all limits upfront you can also just pass `NULL` to `obstack` and
 * in such case we expect that provided `meta` is already seeded with buffers of
 * sizes specified in `limits`.
 *
 * WARNING: The limits when `obstack` is provided are not enforced right now.
 * This is missing implementation not intended state!
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * @param meta: Pointer to the structure where unpacked data will be placed. In
 *   case of integers the value is directly stored. For strings and byte arrays
 *   the data are stored on obstack, if provided, and meta contains pointers to
 *   this data. If you do not provide `obstack` then pointers in the meta need
 * to point to valid buffers.
 * @param limits: Optional pointer to the limits imposed on the meta attributes.
 *   This is used for two purposes: It specifies size of the buffers if you do
 * not provide `obstack; It limits maximal length for data copied to the
 * obstack. Please see `struct rpcmsg_meta_limits` documentation for
 * explanation.
 * @param obstack: pointer to the obstack used to allocate space for received
 *   strings and byte arrays. You can pass `NULL` but in such case you need to
 *   provide your own buffers in `meta`.
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
bool rpcmsg_pack_response_void(cp_pack_t, const struct rpcmsg_meta *meta)
	__attribute__((nonnull));

enum rpcmsg_error {
	RPCMSG_E_NO_ERROR,
	RPCMSG_E_INVALID_REQUEST,
	RPCMSG_E_METHOD_NOT_FOUND,
	RPCMSG_E_INVALID_PARAMS,
	RPCMSG_E_INTERNAL_ERR,
	RPCMSG_E_PARSE_ERR,
	RPCMSG_E_METHOD_CALL_TIMEOUT,
	RPCMSG_E_METHOD_CALL_CANCELLED,
	RPCMSG_E_METHOD_CALL_EXCEPTION,
	RPCMSG_E_UNKNOWN,
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
bool rpcmsg_pack_error(cp_pack_t, const struct rpcmsg_meta *meta,
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
bool rpcmsg_pack_ferror(cp_pack_t, const struct rpcmsg_meta *meta,
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
bool rpcmsg_pack_vferror(cp_pack_t, const struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, va_list args)
	__attribute__((nonnull(1, 2)));

bool rpcmsg_unpack_error(cp_unpack_t unpack, struct cpitem *item,
	enum rpcmsg_error *errnum, char **errmsg) __attribute__((nonnull(1, 2)));

#endif
