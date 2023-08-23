#ifndef SHV_CP_UNPACK_H
#define SHV_CP_UNPACK_H

#include <shv/cp.h>

typedef size_t (*cp_unpack_func_t)(void *ptr, struct cpitem *item);
typedef cp_unpack_func_t *cp_unpack_t;


struct cp_unpack_chainpack {
	cp_unpack_func_t func;
	FILE *f;
};

cp_unpack_t cp_unpack_chainpack_init(struct cp_unpack_chainpack *pack, FILE *f)
	__attribute__((nonnull));

struct cp_unpack_cpon {
	cp_unpack_func_t func;
	FILE *f;
	struct cpon_state state;
};

cp_unpack_t cp_unpack_cpon_init(struct cp_unpack_cpon *pack, FILE *f)
	__attribute__((nonnull));


#define cp_unpack(UNPACK, ITEM) \
	({ \
		cp_unpack_t __unpack = UNPACK; \
		(*__unpack)(__unpack, (ITEM)); \
	})

/*! Instead of getting next item this skips it.
 *
 * The difference between getting and not using the item is that this skips the
 * whole item. That means the whole list or map as well as whole string or blob.
 * This includes multiple calls to the `cp_unpack`.
 *
 * Note that for strings and blobs this only skips all unread bytes from them,
 * it won't continue to the next item unless all bytes were already received.
 * This allows you to use this function to just skip any bytes of data you do
 * not care about and go to the next item.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the last `cp_unpack` call. This is required
 * because there might be unfinished string or blob we need to know about.
 * @returns number of read bytes.
 */
size_t cp_unpack_skip(cp_unpack_t unpack, struct cpitem *item)
	__attribute__((nonnull));

/*! Read until the currently opened container is finished.
 *
 * This is same function such as `cp_unpack_skip` with difference that it stops
 * not after end of the next item, but on container end for which there was no
 * container open. The effect is that opened container is skipped.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the last `cp_unpack` call. This is required
 * because there might be unfinished string or blob we need to know about.
 * @returns number of read bytes.
 */
size_t cp_unpack_finish(cp_unpack_t unpack, struct cpitem *item)
	__attribute__((nonnull));

/*! Copy string to malloc allocated buffer and return it.
 *
 * This unpacks the whole string and returns it.
 *
 * The error can be detected by checking: `item->type == CP_ITEM_STRING`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns malloc allocated null terminated string.
 */
char *cp_unpack_strdup(cp_unpack_t unpack, struct cpitem *item)
	__attribute__((nonnull));

/*! Copy string to the provided buffer.
 *
 * Be aware that compared to the `strncpy` this has a different return!
 *
 * Warning: Null byte is appended only if it fits. You can check for that by
 * comparing returned value and size of the destination.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param dest: Destination buffer where unpacked characters are placed to.
 * @param n: Maximum number of bytes to be used in `dest`.
 * @returns number of bytes written to `dest` or `-1` in case of unpack error.
 */
__attribute__((nonnull(1, 2))) static inline ssize_t cp_unpack_strncpy(
	cp_unpack_t unpack, struct cpitem *item, char *dest, size_t n) {
	item->chr = dest;
	item->bufsiz = n;
	cp_unpack(unpack, item);
	if (item->type != CP_ITEM_STRING)
		return -1;
	if (item->as.String.len < n)
		dest[item->as.String.len] = '\0';
	return item->as.String.len;
}


/*! Copy blob to malloc allocated buffer and provide it.
 *
 * This unpacks the whole blob and returns it.
 *
 * The error can be detected by checking: `item->type == CP_ITEM_BLOB`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data: Pointer where pointer to the data would be placed.
 * @param siz: Pointer where number of valid data bytes were unpacked.
 */
void cp_unpack_memdup(cp_unpack_t unpack, struct cpitem *item, uint8_t **data,
	size_t *siz) __attribute__((nonnull));

/*! Copy blob to the provided buffer.
 *
 * Be aware that compared to the `memcpy` this has a different return!
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param dest: Destination buffer where unpacked bytes are placed to.
 * @param n: Maximum number of bytes to be used in `dest`.
 * @returns number of bytes written to `dest` or `-1` in case of unpack error.
 */
__attribute__((nonnull(1, 2))) static inline ssize_t cp_unpack_memcpy(
	cp_unpack_t unpack, struct cpitem *item, uint8_t *dest, size_t siz) {
	item->buf = dest;
	item->bufsiz = siz;
	cp_unpack(unpack, item);
	if (item->type != CP_ITEM_BLOB)
		return -1;
	return item->as.Blob.len;
}

/*! Open string or blob for reading using stream.
 *
 * This allows you to use `fscanf` and other functions to deal with strings and
 * blobs.
 *
 * To detect if you are dealing with string or blob you need to use `item->type`
 * after you call this function.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns File object or `NULL` in case of unpack error.
 */
FILE *cp_unpack_fopen(cp_unpack_t unpack, struct cpitem *item)
	__attribute__((nonnull));


#endif
