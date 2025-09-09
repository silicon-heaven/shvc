/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCDISCOVER_H
#define SHV_RPCDISCOVER_H
#include <shv/rpcmsg.h>

/**
 * These are functions that are used to pack and unpack RPC Discovery messages.
 *
 * The discovery messages are ``ls`` and ``dir``.
 */

/** RPC Dir flag.
 *
 * This is obsolete and is here only for compatibility reason.
 */
#define RPCDIR_F_NOT_CALLABLE (1 << 0)
/** RPC Dir flag signaling method to be a getter.
 *
 * Getters are methods providing values that have no side effects. They are safe
 * to call automatically.
 */
#define RPCDIR_F_GETTER (1 << 1)
/** RPC Dir flag signaling method to be a setter.
 *
 * Setters are method that accept parameter and provide no value.
 */
#define RPCDIR_F_SETTER (1 << 2)
/** RPC Dir flag signaling that the result will be large.
 *
 * This is commonly combined with :c:macro:`RPCDIR_F_GETTER` to signal that it
 * might not be efficient to call this method in advance to just potentially
 * cache the value.
 */
#define RPCDIR_F_LARGE_RESULT (1 << 3)
/** RPC Dir flag signaling the method is not idempotent.
 *
 * These method are not safe to be used with standard RPC Call implementations
 * that automatically attempt to call method again if response is not received
 * in time.
 */
#define RPCDIR_F_NOT_IDEMPOTENT (1 << 4)
/** RPC Dir flag signaling that method requires UserID.
 *
 * UserID is optional field that is filled in by SHV RPC Brokers only if clients
 * provides it initially. Most of the methods do not need it and thus it is
 * desirable to include it only if method really needs it. That is what this
 * flag signals for the method.
 */
#define RPCDIR_F_USERID_REQUIRED (1 << 5)

/** Keys for the dir's method description IMap. */
enum rpcdir_keys {
	RPCDIR_KEY_NAME = 1,
	RPCDIR_KEY_FLAGS = 2,
	RPCDIR_KEY_PARAM = 3,
	RPCDIR_KEY_RESULT = 4,
	RPCDIR_KEY_ACCESS = 5,
	RPCDIR_KEY_SIGNALS = 6,
	RPCDIR_KEY_EXTRA = 63,
};

/** The representation of method in SHV RPC Discovery API. */
struct rpcdir {
	/** Method name */
	const char *name;
	/** Parameter type description. It is ``NULL`` in case no parameter is
	 * expected.
	 */
	const char *param;
	/** Result type description. It is ``NULL`` in case no result is provided. */
	const char *result;
	/** Bitwise combination of the ``RPCDIR_F_*`` flags. */
	int flags;
	/** Minimal access level needed for this method. */
	rpcaccess_t access;
	/** Pair of the signal's name and its parameter type description.
	 *
	 * This is used exclusively in :c:struct:`rpcdir`.
	 */
	struct rpcdirsig {
		/** Signal name */
		const char *name;
		/** Parameter type description. It is ``NULL`` in case
		 * :c:var:`rpcdir.result` is the parameter to be provided.
		 */
		const char *param;
	}
	/** Signals associated with this method.
	 *
	 * This must be an array of size :c:var:`rpcdir.signals_cnt`.
	 */
	const *signals;
	/** Size of the :c:var:`rpcdir.signals` array. */
	size_t signals_cnt;
};

/** Method description for the standard ``ls`` and ``dir`` methods. */
extern struct rpcdir rpcdir_ls, rpcdir_dir;

/** Pack dir's method description.
 *
 * :param pack: The generic packer where it is about to be packed.
 * :param method: The method description to be packed.
 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
 */
[[gnu::nonnull]]
bool rpcdir_pack(cp_pack_t pack, const struct rpcdir *method);

/** Unpack dir's method description.
 *
 * This stores the whole description in the obstack. To everything (including
 * what was allocated in the obstack after call to this function) you can use
 * the result of this function from obstack.
 *
 * :param unpack: The generic unpacker to be used to unpack items.
 * :param item: The item instance that was used to unpack the previous item.
 * :param obstack: Pointer to the obstack instance to be used to allocate
 *   method descrition.
 * :return: Pointer to the method description or ``NULL`` in case of an unpack
 *   error. You can investigate ``item`` to identify the failure cause.
 */
[[gnu::nonnull]]
struct rpcdir *rpcdir_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);


#endif
