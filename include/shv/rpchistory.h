/* SPDX-License-Identifier: MIT */
#ifndef _RPCHISTORY_H
#define _RPCHISTORY_H
#include <obstack.h>
#include <shv/rpcdir.h>

/*! @file
 * These are functions and defines that are used to pack and unpack
 * libshvhistory nodes.
 */

/*! Possible @ref rpchistory_record_head types */
enum rpchistory_record_types {
	/*! Normal record. */
	RPCHISTORY_RECORD_NORMAL = 1,
	/*! Keep record.
	 *
	 *  These are normal records repeated with newer date and time if there is
	 *  no update of them in past number of records.
	 */
	RPCHISTORY_RECORD_KEEP,
	/*! Timejump record.
	 *
	 *  This is information that all previous recorded times should actually be
	 *  considered to be with time modification. The time offset is specified in
	 *  timestamp field of @ref rpchistory_record_head structure.
	 */
	RPCHISTORY_RECORD_TIMEJUMP,
	/*! Time ambiguity record.
	 *
	 *  This is information that date and time of the new logs has no relevance
	 *  compared to the previous ones.
	 */
	RPCHISTORY_RECORD_TIMEABIG,
};

/*! Keys for the fetch's method description IMap. */
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

/*! Structure defining record' head for SHV communication.
 *
 *  This is a represenation of record's head. This includes all record's
 *  information except for data/value. This can be used together with
 *  @ref rpchistory_record_pack_begin function to pack the head.
 */
struct rpchistory_record_head {
	/*! Record type.
	 *
	 *  Refer to @ref rpchistory_record_types for possible types.
	 */
	int type;
	/*! Access Level. */
	rpcaccess_t access;
	/*! Number of seconds of time skip. */
	int timejump;
	/*! Repeat carried by signal message. */
	bool repeat;
	/*! SHV path to the node relative to the .history parrent. */
	const char *path;
	/*! Signal name. */
	const char *signal;
	/*! Signal's associated method name. */
	const char *source;
	/*! UserID carried by signal message. */
	const char *userid;
	/*! DateTime of system when record was created. */
	struct cpdatetime datetime;
};

/*! Description for fetch method. */
extern struct rpcdir rpchistory_fetch;
/*! Description for span method. */
extern struct rpcdir rpchistory_span;
/*! Description for getLog method. */
extern struct rpcdir rpchistory_getlog;
/*! Description for sync method. */
extern struct rpcdir rpchistory_sync;
/*! Description for lastSync method. */
extern struct rpcdir rpchistory_lastsync;

/*! Begin iMap packing of the record.
 *
 * This packs all record's values except for data. The values are passed
 * with @ref rpchistory_record_head structure and are packed as iMap.
 * The user should pack the data after this call.
 *
 * @param pack: The generic packer where it is about to be packed.
 * @param head: The pointer to @ref rpchistory_record_pack_begin structure.
 * @returns `false` if packing encounters failure and `true` otherwise.
 */
bool rpchistory_record_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head);

/*! Finish iMap packing of the record.
 *
 * @param pack: The generic packer where it is about to be packed.
 * @returns `false` if packing encounters failure and `true` otherwise.
 */
bool rpchistory_record_pack_end(cp_pack_t pack);

#endif /* _RPCHISTORY_H */
