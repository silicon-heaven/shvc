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

enum rpcmsg_error_key { RPCMSG_E_KEY_CODE = 1, RPCMSG_E_KEY_MESSAGE };

/*! Pack request message meta and open imap. The followup packed data are
 * parameters provided to the method call request. The message needs to be
 * terminated with container end (`chainpack_pack_container_end`).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node the method we want to request is associated
 * with.
 * @param method: name of the method we request to call.
 * @param rid: request identifier. Thanks to this number you can associate
 * response with requests.
 */
void rpcmsg_pack_request(cp_pack_t, const char *path, const char *method,
	int64_t rid) __attribute__((nonnull));

/*! Pack signal message meta and open imap. The followup packed data are
 * signaled values. The message needs to be terminated with container end
 * (`chainpack_pack_container_end`).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node method is associated with.
 * @param method: name of the method signal is raised for.
 */
void rpcmsg_pack_signal(cp_pack_t, const char *path, const char *method)
	__attribute__((nonnull));

/*! Pack value change signal message meta and open imap. The followup packed
 * data are signaled values. The message needs to be terminated with container
 * end (`chainpack_pack_container_end`).
 *
 * This is actually just a convenient way to call `rpcmsg_pack_signal` for
 * "chng" methods.
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path the value change signal is associated with.
 */
static inline void rpcmsg_pack_chng(cp_pack_t pack, const char *path) {
	rpcmsg_pack_signal(pack, path, "chng");
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
 * @retunrs Pointer to string constant representing the access level.
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
	size_t path;
	/*! Limit on `method` size.
	 *
	 * The same suggestion applies here as for `path`.
	 */
	size_t method;
	/*! Limit on `cids.ptr` buffer.
	 *
	 * It is highly suggested to set this to `-1` or some high number if you use
	 * obstack. That is because messages without client IDs can't be responded
	 * due to the not arriving to the source without this reverse path.
	 *
	 * In case you do not use obstack then set buffer size of `cids.ptr` as
	 * usual.
	 */
	size_t cids;
};

/*! Unpack meta of the RPC message.
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
 * case of integers the value is directly stored. For strings and byte arrays
 * the data are stored on obstack, if provided, and meta contains pointers to
 * this data. If you do not provide `obstack` then pointers in the meta need to
 * point to valid buffers.
 * @param limits: Optional pointer to the limits imposed on the meta attributes.
 * This is used for two purposes: It specifies size of the buffers if you do not
 * provide `obstack; It limits maximal length for data copied to the obstack.
 * Please see `struct rpcmsg_meta_limits` documentation for explanation.
 * @param obstack: pointer to the obstack used to allocate space for received
 * strings and byte arrays. You can pass `NULL` but in such case you need to
 * provide your own buffers in `meta`.
 * @returns `false` in case unpack reports error, if meta contains invalid value
 * for supported key, or when there is not enough space to store caller IDs. In
 * other cases `true` is returned. In short this return primarily signals if it
 * is possible to generate response to this message for request messages.
 */
bool rpcmsg_meta_unpack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta *meta, struct rpcmsg_meta_limits *limits,
	struct obstack *obstack);

void rpcmsg_pack_response(cp_pack_t, struct rpcmsg_meta *meta)
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

void rpcmsg_pack_error(cp_pack_t, struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *msg) __attribute__((nonnull(1, 2)));

void rpcmsg_pack_ferror(cp_pack_t, struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, ...) __attribute__((nonnull(1, 2)));

void rpcmsg_pack_vferror(cp_pack_t, struct rpcmsg_meta *meta,
	enum rpcmsg_error error, const char *fmt, va_list args)
	__attribute__((nonnull(1, 2)));


#if 0

/*! String handle that can be increased in size and reused for the future
 * strings. It is used by `rpcmsg_unpack_str` and by other functions that
 * internally call that function. It provides a growable string with known size
 * limitation.
 *
 * You should preferably allocate this using `malloc` because
 * `rpcmsg_unpack_str` will call `realloc` on it (with exception that is
 * described in the `siz` field description).
 */
struct rpcclient_str {
	/*! Size of the `str` in bytes. You can set this to a negative number to
	 * prevent `rpcmsg_unpack_str` from reallocating. This provides you either
	 * ceiling functionality or a way to pass non-malloc allocated memory.
	 */
	ssize_t siz;
	/*! Actual characters of the string. */
	char str[];
};

/*! Unpack string to `rpcmsg_str`.
 *
 */
bool rpcclient_unpack_str(rpcclient_t, struct rpcclient_str **str);

/*! Check if `rpcmsg_str` has truncated content.
 *
 * `rpcmsg_unpack_str` might not be able to store the whole string if you set
 * `siz` to some negative number as that prevents reallocation. In such case you
 * can end up with truncated string and this method detects it. You should
 * always use it before you call C string methods (that expect `NULL`
 * termination) to check validity.
 *
 * @param str: pointer to the `rpcmsg_str` that was previously passed to
 * `rpcmsg_unpack_str`.
 * @returns: `true` if string is truncated or otherwise invalid and `false`
 * otherwise.
 */
bool rpcclient_str_truncated(const struct rpcclient_str *str)
	__attribute__((nonnull));



bool rpcmsg_unpack_access(rpcclient_t, enum rpcmsg_access *access);

const char *rpcmsg_access_str(enum rpcmsg_access);

struct rpcmsg_request_info {
	/* Access level assigned by SHV Broker */
	enum rpcmsg_access acc_grant;
	/* Request ID */
	int64_t rid;
	/* Client IDs */
	size_t cidscnt, rcidscnt;
	// TODO ssize_t and allow no realloc trough it?
	size_t cidssiz;
	int64_t cids[];
};

/*! Unpack message meta and open imap. The followup data depend on type returned
 * by this function.
 *
 * @param unpack: unpack context the meta should be read from.
 * @param path: pointer to the variable where SHV path is stored.
 * @param method: name of the method signal is raised for.
 */
enum rpcmsg_type {
	/*! The message's meta is invalid or unpack is in error state. This is an
	 * error state and you most likely want to perform reconnect or connection
	 * reset.
	 */
	RPCMSG_T_INVALID,
	RPCMSG_T_REQUEST,
	RPCMSG_T_RESPONSE,
	RPCMSG_T_ERROR,
	RPCMSG_T_SIGNAL,
} rpcmsg_unpack(rpcclient_t, struct rpcmsg_str **path,
	struct rpcmsg_str **method, struct rpcmsg_request_info **rinfo);


#endif
#endif
