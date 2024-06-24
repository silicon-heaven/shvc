/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCFILE_H
#define SHV_RPCFILE_H
/*! @file
 * These are functions that are used to pack and unpack File nodes.
 *
 * This is to simplify the packing and unpacking of some more complicated
 * methods.
 */

#include <shv/rpcdir.h>

/*! Size of SHA1 result.
 *
 * The size of SHA1 hash function must always be 20 bytes.
 */
#define RPCFILE_SHA1_SIZE (20)

/*! File nodes acess levels */
enum rpcfile_access {
	/*! Allow CRC32/SHA1 validations. Must be implemented allongsize
	 *  read. This is automatically allowed if @ref RPCFILE_ACCESS_READ
	 *  is set.
	 */
	RPCFILE_ACCESS_VALIDATION = 1,
	/*! Allow read operations. This sets @ref RPCFILE_ACCESS_VALIDATION */
	RPCFILE_ACCESS_READ = 2,
	/*! Allow write operations. Write outside of file boundary is up to
	 *  the implementation.
	 */
	RPCFILE_ACCESS_WRITE = 4,
	/*! Allow file truncate. This should be allowed only if write outside of
	 *  file boundary is allowed. If truncate is allowed, then write
	 *  @ref RPCFILE_ACCESS_WRITE is allowed as well.
	 */
	RPCFILE_ACCESS_TRUNCATE = 8,
	/*! Allow file append. This may be provided even
	 *  if @ref RPCFILE_ACCESS_WRITE is not. The write with append is always
	 *  outside of file boundary.
	 */
	RPCFILE_ACCESS_APPEND = 16,
};

/*! Keys for the stat's method description IMap. */
enum rpcfile_stat_keys {
	RPCFILE_STAT_KEY_TYPE = 0,
	RPCFILE_STAT_KEY_SIZE = 1,
	RPCFILE_STAT_KEY_PAGESIZE = 2,
	RPCFILE_STAT_KEY_ACCESSTIME = 3,
	RPCFILE_STAT_KEY_MODTIME = 4,
	RPCFILE_STAT_KEY_MAXWRITE = 5,
};

/*! Supported types of file */
enum rpcfile_stat_type {
	/*! Regular file. */
	RPCFILE_STAT_TYPE_REGULAR = 0,
};

/*! The representation of file node statistics. */
struct rpcfile_stat_s {
	/*! Type of the file. Currently only 0 is valid. */
	int type;
	/*! Size of the file in bytes. */
	int size;
	/*! Page size (ideal size and thus alignment for this file efficient
	 * access). */
	int page_size;
	/*! Optional maximal size in bytes of a single write that is accepted. */
	int max_write;
	/*! Optional time of latest data access. */
	struct cpdatetime access_time;
	/*! Optional time of latest data modification. */
	struct cpdatetime mod_time;
};

/*! Description for stat method */
extern struct rpcdir rpcfile_stat;
/*! Description for size method */
extern struct rpcdir rpcfile_size;
/*! Description for crc method */
extern struct rpcdir rpcfile_crc;
/*! Description for sha1 method */
extern struct rpcdir rpcfile_sha1;
/*! Description for read method */
extern struct rpcdir rpcfile_read;
/*! Description for write method */
extern struct rpcdir rpcfile_write;
/*! Description for truncate method */
extern struct rpcdir rpcfile_truncate;
/*! Description for append method */
extern struct rpcdir rpcfile_append;

/*! Pack stat's method response.
 *
 * @param pack: The generic packer where it is about to be packed.
 * @param stats: The pointer to rpcfile_stat_s containing file statistics.
 * @returns `false` if packing encounters failure and `true` otherwise.
 */
bool rpcfile_stat_pack(cp_pack_t pack, struct rpcfile_stat_s *stats)
	__attribute__((nonnull));

/*! Unpack stat's method response.
 *
 * @param unpack: The generic unpacker to be used to unpack items.
 * @param item: The item instance that was used to unpack the previous item.
 * @param obstack: Pointer to the obstack instance to be used to allocate
 *   file statistics.
 * @returns Pointer to the file statistics or `NULL` in case of an unpack
 *   error. You can investigate `item` to identify the failure cause.
 */
struct rpcfile_stat_s *rpcfile_stat_unpack(cp_unpack_t unpack,
	struct cpitem *item, struct obstack *obstack) __attribute__((nonnull));

#endif /* SHV_RCPFILE_H */
