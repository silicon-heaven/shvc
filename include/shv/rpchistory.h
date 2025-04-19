/* SPDX-License-Identifier: MIT */
#ifndef _RPCHISTORY_H
#define _RPCHISTORY_H
#include <obstack.h>
#include <shv/rpcdir.h>

/**
 * These are functions and defines that are used to pack and unpack
 * libshvhistory nodes.
 */

/** Possible :c:struct:`rpchistory_record_head` types */
enum rpchistory_record_types {
	/** Normal record. */
	RPCHISTORY_RECORD_NORMAL = 1,
	/** Keep record.
	 *
	 * These are normal records repeated with newer date and time if there is
	 * no update of them in past number of records.
	 */
	RPCHISTORY_RECORD_KEEP,
	/** Timejump record.
	 *
	 * This is information that all previous recorded times should actually be
	 * considered to be with time modification. The time offset is specified in
	 * timestamp field of :c:struct:`rpchistory_record_head` structure.
	 */
	RPCHISTORY_RECORD_TIMEJUMP,
	/** Time ambiguity record.
	 *
	 * This is information that date and time of the new logs has no relevance
	 * compared to the previous ones.
	 */
	RPCHISTORY_RECORD_TIMEABIG,
};

/** Keys for the fetch's method description IMap. */
enum rpchistory_fetch_keys {
	RPCHISTORY_FETCH_KEY_TYPE = 0,
	RPCHISTORY_FETCH_KEY_TIMESTAMP = 1,
	RPCHISTORY_FETCH_KEY_PATH = 2,
	RPCHISTORY_FETCH_KEY_SIGNAL = 3,
	RPCHISTORY_FETCH_KEY_SOURCE = 4,
	RPCHISTORY_FETCH_KEY_VALUE = 5,
	RPCHISTORY_FETCH_KEY_ACCESSLEVEL = 6,
	RPCHISTORY_FETCH_KEY_USERID = 7,
	RPCHISTORY_FETCH_KEY_REPEAT = 8,
	RPCHISTORY_FETCH_KEY_TIMEJUMP = 60,
};

/** Keys for the getLog's method response description IMap. */
enum rpchistory_getlog_keys {
	RPCHISTORY_GETLOG_KEY_TIMESTAMP = 1,
	RPCHISTORY_GETLOG_KEY_REF = 2,
	RPCHISTORY_GETLOG_KEY_PATH = 3,
	RPCHISTORY_GETLOG_KEY_SIGNAL = 4,
	RPCHISTORY_GETLOG_KEY_SOURCE = 5,
	RPCHISTORY_GETLOG_KEY_VALUE = 6,
	RPCHISTORY_GETLOG_KEY_USERID = 7,
	RPCHISTORY_GETLOG_KEY_REPEAT = 8,
};

/** Structure defining record' head for SHV communication.
 *
 * This is a represenation of record's head. This includes all record's
 * information except for data/value. This can be used together with
 * :c:func:`rpchistory_record_pack_begin` function to pack the head.
 */
struct rpchistory_record_head {
	/** Record type.
	 *
	 * Refer to :c:enum:`rpchistory_record_types` for possible types.
	 */
	int type;
	/** Access Level. */
	rpcaccess_t access;
	/** Number of seconds of time skip. */
	int timejump;
	/** Repeat carried by signal message. */
	bool repeat;
	/** SHV path to the node relative to the .history parrent. */
	const char *path;
	/** Signal name. */
	const char *signal;
	/** Signal's associated method name. */
	const char *source;
	/** UserID carried by signal message. */
	const char *userid;
	/** DateTime of system when record was created. */
	struct cpdatetime datetime;
};

/** This structure provides fields for getLog method request.
 *
 * This structure is returned as a result of
 * :c:func:`rpchistory_getlog_request_unpack` call.
 */
struct rpchistory_getlog_request {
	/** This is a :c:struct:`cpdatetime` since logs should be provided. */
	struct cpdatetime since;
	/** This is a :c:struct:`cpdatetime` until logs should be provided. */
	struct cpdatetime until;
	/** Optional Int as a limitation for the number of records to be at most
	 * returned.
	 *
	 * .. NOTE::
	 *   ``UINT_MAX`` is used as a default when no limit is specified.
	 */
	unsigned count;
	/** This controls if virtual records should be inserted at the start that
	 * copy state of the signals. This provides fixed point to start when you
	 * for example plotting data. These records are virtual and are not actually
	 * captured signals.
	 */
	bool snapshot;
	/** RPC RI that should be used to match ``path:method:signal``. */
	const char *ri;
};

/** Description for fetch method. */
extern const struct rpcdir rpchistory_fetch;
/** Description for span method. */
extern const struct rpcdir rpchistory_span;
/** Description for getLog method. */
extern const struct rpcdir rpchistory_getlog;
/** Description for sync method. */
extern const struct rpcdir rpchistory_sync;
/** Description for lastSync method. */
extern const struct rpcdir rpchistory_lastsync;

/** Begin iMap packing of getLog method response.
 *
 * This packs all record's values except for data. The values are passed
 * with :c:struct:`rpchistory_record_head` structure and are packed as iMap.
 * The user should pack the data after this call.
 *
 * :param pack: The generic packer where it is about to be packed.
 * :param head: The pointer to :c:struct:`rpchistory_record_head` structure.
 * :param ref: This provides a way to reference the previous record to use it as
 *   the default for path, signal and source instead of obtaining them from
 *   :c:struct:`rpchistory_record_head` structure. It is Int where 0 is record
 *   right before this one in the list. The offset must always be to the most
 *   closest record that specifies desired default.
 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
 */
bool rpchistory_getlog_response_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head, int ref);

/** Finish iMap packing of getLog method response..
 *
 * :param pack: The generic packer where it is about to be packed.
 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
 */
bool rpchistory_getlog_response_pack_end(cp_pack_t pack);

/** Unpack getLog method request.
 *
 * :param unpack: The generic unpacker to be used to unpack items.
 * :param item: The item instance that was used to unpack the previous item.
 * :param obstack: Pointer to the obstack instance to be used to allocate
 *   :c:struct:`rpchistory_getlog_request` structure.
 * :return: Pointer to :c:struct:`rpchistory_getlog_request` or ``NULL`` in case
 *   of an unpack error. You can investigate `item` to identify the failure
 *   cause.
 */
struct rpchistory_getlog_request *rpchistory_getlog_request_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);

/** Begin iMap packing of the record.
 *
 * This packs all record's values except for data. The values are passed
 * with :c:struct:`rpchistory_record_head` structure and are packed as iMap.
 * The user should pack the data after this call.
 *
 * :param pack: The generic packer where it is about to be packed.
 * :param head: The pointer to :c:struct:`rpchistory_record_pack_begin`
 *   structure.
 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
 */
bool rpchistory_record_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head);

/** Finish iMap packing of the record.
 *
 * :param pack: The generic packer where it is about to be packed.
 * :return: ``false`` if packing encounters failure and ``true`` otherwise.
 */
bool rpchistory_record_pack_end(cp_pack_t pack);

#endif /* _RPCHISTORY_H */
