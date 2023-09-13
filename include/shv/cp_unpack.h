#ifndef SHV_CP_UNPACK_H
#define SHV_CP_UNPACK_H

#include <shv/cp.h>
#include <obstack.h>

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

#define cp_unpack_expect_type(UNPACK, ITEM, TYPE) \
	({ \
		struct cpitem *__item = ITEM; \
		cp_unpack(UNPACK, __item); \
		__item->type == TYPE; \
	})

/*! Instead of getting next item this drops the any unread blocks from current
 * one. The effect is that next unpack will unpack a new item instead of next
 * block of the current one.
 *
 * This is here for strings and blobs and only skips all unread bytes from them,
 * it won't continue to the next item. This allows you to use this function to
 * just skip any bytes of data you do not care about and go to the next item.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns number of read bytes.
 */
size_t cp_unpack_drop(cp_unpack_t unpack, struct cpitem *item)
	__attribute__((nonnull));

/*! Instead of getting next item this skips it.
 *
 * The difference between getting and not using the item is that this skips the
 * whole item. That means the whole list or map as well as whole string or blob.
 * This includes multiple calls to the `cp_unpack`.
 *
 * This also fist drop any unfinished string or blob with `cp_unpack_drop` to
 * for sure finish the previous item.
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
 * This is same function as `cp_unpack_skip` with difference that it stops not
 * after end of the next item, but on container end for which there was no
 * container open. The effect is that opened container is skipped.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the last `cp_unpack` call. This is required
 * because there might be unfinished string or blob we need to know about.
 * @param depth: How many containers we should finish. To finish one pass `1`.
 * To close two containers pass `2` and so on. The `0` provides the same
 * functionality as `cp_unpack_skip`.
 * @returns number of read bytes.
 */
size_t cp_unpack_finish(cp_unpack_t unpack, struct cpitem *item, unsigned depth)
	__attribute__((nonnull));

#define cp_extract_int(ITEM, DEST) \
	({ \
		struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_INT) { \
			if (sizeof(DEST) < sizeof(long long)) { \
				long long __lim = 1LL << ((sizeof(DEST) * 8) - 1); \
				__valid = __item->as.Int >= -__lim && __item->as.Int < __lim; \
			} else \
				__valid = true; \
			(DEST) = __item->as.Int; \
		} \
		__valid; \
	})

#define cp_extact_uint(ITEM, DEST) \
	({ \
		struct cpitem *__item = ITEM; \
		bool __valid = false; \
		if (__item->type == CPITEM_UINT) { \
			if (sizeof(DEST) < sizeof(unsigned long long)) { \
				unsigned long long __lim = 1LL << ((sizeof(DEST) * 8) - 1); \
				__valid = __item->as.Int < __lim; \
			} else \
				__valid = true; \
			(DEST) = __item->as.Int; \
		} \
		__valid; \
	})

#define cp_unpack_int(UNPACK, ITEM, DEST) \
	({ \
		struct cpitem *__uitem = ITEM; \
		ssize_t res = cp_unpack(UNPACK, __uitem); \
		cp_extract_int(__uitem, DEST) ? res : -res; \
	})

#define cp_unpack_uint(UNPACK, ITEM, DEST) \
	({ \
		struct cpitem *__uitem = ITEM; \
		ssize_t res = cp_unpack(UNPACK, __uitem); \
		cp_extract_uint(__uitem, DEST) ? res : -res; \
	})

/*! Unpack a single by from string.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns Unpacked byte or `-1` in case of an unpack error.
 */
__attribute__((nonnull)) static inline int cp_unpack_getc(
	cp_unpack_t unpack, struct cpitem *item) {
	uint8_t res = 0;
	item->buf = &res;
	item->bufsiz = 1;
	cp_unpack(unpack, item);
	if (item->type != CPITEM_STRING)
		return -1;
	return res;
}

/*! Copy string to malloc allocated buffer and return it.
 *
 * This unpacks the whole string and returns it.
 *
 * The error can be detected by checking: `item->type == CPITEM_STRING`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns malloc allocated null terminated string.
 */
char *cp_unpack_strdup(cp_unpack_t unpack, struct cpitem *item)
	__attribute__((nonnull));

/*! Copy string of at most given length to malloc allocated buffer and return
 * it.
 *
 * This unpacks the string up to the given length and returns it. It always
 * appends '\0'.
 *
 * The error can be detected by checking: `item->type == CPITEM_STRING`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param len: Maximum number of bytes to be copied.
 * @returns malloc allocated null terminated string.
 */
char *cp_unpack_strndup(cp_unpack_t unpack, struct cpitem *item, size_t len)
	__attribute__((nonnull));

/*! Copy string to obstack allocated buffer and return it.
 *
 * This is same as @ref cp_unpack_strdup except it uses obstack to allocated the
 * required space.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param obstack: Obstack used to allocate the space needed for the string
 * object.
 * @returns pointer to the allocated string object.
 */
char *cp_unpack_strdupo(cp_unpack_t unpack, struct cpitem *item,
	struct obstack *obstack) __attribute__((nonnull));

/*! Copy string up to given length to obstack allocated buffer and return it.
 *
 * This is same as @ref cp_unpack_strndup except it uses obstack to allocated
 * the required space.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param obstack: Obstack used to allocate the space needed for the string
 * object.
 * @param len: Maximum number of bytes to be copied.
 * @returns pointer to the allocated string object.
 */
char *cp_unpack_strndupo(cp_unpack_t unpack, struct cpitem *item, size_t len,
	struct obstack *obstack) __attribute__((nonnull));

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
	if (item->type != CPITEM_STRING) {
		item->bufsiz = 0;
		return -1;
	}
	if (item->as.String.len < n)
		dest[item->as.String.len] = '\0';
	item->bufsiz = 0;
	return item->as.String.len;
}

/*! Expect one of the strings to be present and provide index to say which.
 *
 * Parsing of maps is a common operation. Regularly keys can be in any order and
 * thus we need to expect multiple different strings to be present. This handles
 * that for you.
 *
 * Note that the implementation is based on decision tree that is generated in
 * runtime. If you know the strings upfront you can get a better performance
 * if you fetch bytes and use some hash searching algorithm instead.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 */
int cp_unpack_expect_str(cp_unpack_t unpack, struct cpitem *item,
	const char **strings) __attribute__((nonnull));


/*! Copy blob to malloc allocated buffer and provide it.
 *
 * This unpacks the whole blob and returns it.
 *
 * The error can be detected by checking: `item->type == CPITEM_BLOB`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data: Pointer where pointer to the data would be placed.
 * @param siz: Pointer where number of valid data bytes were unpacked.
 */
void cp_unpack_memdup(cp_unpack_t unpack, struct cpitem *item, uint8_t **data,
	size_t *siz) __attribute__((nonnull));

/*! Copy blob up to given number of bytes to malloc allocated buffer and provide
 * it.
 *
 * This unpacks the blob up to the given size and returns it.
 *
 * The error can be detected by checking: `item->type == CPITEM_BLOB`.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data: Pointer where pointer to the data would be placed.
 * @param siz: Pointer to maximum number of bytes to be copied that is updated
 * with number of valid bytes actually copied over.
 */
void cp_unpack_memndup(cp_unpack_t unpack, struct cpitem *item, uint8_t **data,
	size_t *siz) __attribute__((nonnull));

/*! Copy blob to obstack allocated buffer and return it.
 *
 * This is same as @ref cp_unpack_memdup except it uses obstack to allocated the
 * required space.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data: Pointer where pointer to the data would be placed.
 * @param siz: Pointer where number of valid data bytes were unpacked.
 * @param obstack: Obstack used to allocate the space needed for the blob
 * object.
 */
void cp_unpack_memdupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack) __attribute__((nonnull));

/*! Copy blob up to given number of bytes to obstack allocated buffer and return
 * it.
 *
 * This is same as @ref cp_unpack_memndup except it uses obstack to allocated
 * the required space.
 *
 * @param unpack: Unpack handle.
 * @param item: Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data: Pointer where pointer to the data would be placed.
 * @param siz: Pointer to maximum number of bytes to be copied that is updated
 * with number of valid bytes actually copied over.
 * @param obstack: Obstack used to allocate the space needed for the blob
 * object.
 */
void cp_unpack_memndupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack) __attribute__((nonnull));

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
	if (item->type != CPITEM_BLOB)
		return -1;
	return item->as.Blob.len;
}

#define for_cp_unpack_list(UNPACK, ITEM) \
	while (({ \
		struct cpitem *__item = ITEM; \
		cp_unpack(UNPACK, __item); \
		__item->type != CPITEM_INVALID && __item->type != CPITEM_CONTAINER_END; \
	}))

#define for_cp_unpack_ilist(UNPACK, ITEM, CNT) \
	for (unsigned CNT = 0; ({ \
			 struct cpitem *__item = ITEM; \
			 cp_unpack(UNPACK, __item); \
			 __item->type != CPITEM_INVALID && \
				 __item->type != CPITEM_CONTAINER_END; \
		 }); \
		 CNT++)

#define for_cp_unpack_map(UNPACK, ITEM) \
	while (({ \
		struct cpitem *__item = ITEM; \
		cp_unpack(UNPACK, __item); \
		__item->type == CPITEM_STRING; \
	}))

#define for_cp_unpack_imap(UNPACK, ITEM) \
	while (({ \
		struct cpitem *__item = ITEM; \
		cp_unpack(UNPACK, __item); \
		__item->type == CPITEM_INT; \
	}))

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
