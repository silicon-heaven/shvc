#ifndef SHV_RPCMSG_H
#define SHV_RPCMSG_H

#include <stdbool.h>
#include <sys/types.h>
#include <shv/rpcclient.h>

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
void rpcmsg_pack_request(cpcp_pack_context *pack, const char *path,
	const char *method, int64_t rid) __attribute__((nonnull));

/*! Pack signal message meta and open imap. The followup packed data are
 * signaled values. The message needs to be terminated with container end
 * (`chainpack_pack_container_end`).
 *
 * @param pack: pack context the meta should be written to.
 * @param path: SHV path to the node method is associated with.
 * @param method: name of the method signal is raised for.
 */
void rpcmsg_pack_signal(cpcp_pack_context *pack, const char *path,
	const char *method) __attribute__((nonnull));

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
static inline void rpcmsg_pack_chng(cpcp_pack_context *pack, const char *path) {
	rpcmsg_pack_signal(pack, path, "chng");
}


/*! String handle that can be increased in size and reused for the future
 * strings. It is used by `rpcmsg_unpack_str` and by other functions that
 * internally call that function. It provides a growable string with known size
 * limitation.
 *
 * You should preferably allocate this using `malloc` because
 * `rpcmsg_unpack_str` will call `realloc` on it (with exception that is
 * described in the `siz` field description).
 */
struct rpcmsg_str {
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
bool rpcmsg_unpack_str(struct rpcclient_msg msg, struct rpcmsg_str **str);

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
bool rpcmsg_str_truncated(const struct rpcmsg_str *str) __attribute__((nonnull));


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

bool rpcmsg_unpack_access(struct rpcclient_msg msg, enum rpcmsg_access *access);

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
} rpcmsg_unpack(struct rpcclient_msg msg, struct rpcmsg_str **path,
	struct rpcmsg_str **method, struct rpcmsg_request_info **rinfo);


#endif
