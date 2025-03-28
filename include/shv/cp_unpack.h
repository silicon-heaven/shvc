/* SPDX-License-Identifier: MIT */
#ifndef SHV_CP_UNPACK_H
#define SHV_CP_UNPACK_H
/*! @file
 * Generic unpacker API with utility functions to unpack data more easilly.
 */

#include <obstack.h>
#include <shv/cp.h>

/*! Definition of function that provides generic unpacker.
 *
 * This function is called to unpack next item and all its context, including
 * the input, is provided by **ptr**.
 *
 * This abstraction allows us to work with any unpacker in the same way and it
 * also provides a way to overlay additional handling on top of the low level
 * unpacker (such as logging).
 *
 * @param ptr Pointer to the context information that is pointer to the @ref
 *   cp_unpack_t.
 * @param item Item where info about the unpacked item and its value is placed
 *   to.
 */
typedef void (*cp_unpack_func_t)(void *ptr, struct cpitem *item);
/*! Generic unpacker.
 *
 * This is pointer to the function pointer that implements unpacking. The
 * function is called by dereferencing this generic unpacker and the pointer to
 * it is passed to the function as the first argument. This double pointer
 * provides you a way to store any context info side by pointer to the unpack
 * function.
 *
 * To understand this you can look into the @ref cp_unpack_chainpack and @ref
 * cp_unpack_cpon definitions. They have @ref cp_unpack_func_t as a first field
 * and thus this function gets pointer to the structure in the first argument.
 */
typedef cp_unpack_func_t *cp_unpack_t;


/*! Handle for the ChainPack generic unpacker. */
struct cp_unpack_chainpack {
	/*! Generic unpacker function. */
	cp_unpack_func_t func;
	/*! File object used in @ref chainpack_unpack() calls. */
	FILE *f;
};

/*! Initialize @ref cp_unpack_chainpack.
 *
 * The initialization only fills in @ref cp_unpack_chainpack.func and sets @ref
 * cp_unpack_chainpack.f to the **f** passed to it. There is no need for a
 * special resource deallocation afterward.
 *
 * @param unpack Pointer to the handle to be initialized.
 * @param f File used to read ChainPack bytes.
 * @returns Generic unpacker.
 */
[[gnu::nonnull]]
cp_unpack_t cp_unpack_chainpack_init(struct cp_unpack_chainpack *unpack, FILE *f);

/*! Handle for the CPON generic unpacker. */
struct cp_unpack_cpon {
	/*! Generic unpacker function. */
	cp_unpack_func_t func;
	/*! File object used in @ref cpon_unpack calls. */
	FILE *f;
	/*! Context information state for the CPON.
	 *
	 * Function @ref cp_unpack_cpon_init() will prepare this state for the
	 * unconstrained allocation. If you prefer to limit the depth you can change
	 * the @ref cpon_state.realloc to your own implementation.
	 *
	 * After generic unpacker end of use you need to free @ref cpon_state.ctx!
	 */
	struct cpon_state state;
};

/*! Initialize @ref cp_unpack_cpon.
 *
 * The initialization only fills in @ref cp_unpack_chainpack.func, sets @ref
 * cp_unpack_chainpack.f to the **f** passed to it, initializes @ref cpon_state
 * to zeroes and sets @ref cpon_state.realloc to unconstrained allocator.
 *
 * Remember to release resources allocated by @ref cpon_state.ctx!
 *
 * @param unpack Pointer to the handle to be initialized.
 * @param f File used to read CPON bytes.
 * @returns Generic unpacker.
 */
[[gnu::nonnull]]
cp_unpack_t cp_unpack_cpon_init(struct cp_unpack_cpon *unpack, FILE *f);


/*! Unpack item with generic unpacker.
 *
 * The calling is described in @ref cp_unpack_func_t. You want to repeatedly
 * pass the same @ref cpitem instance to unpack multiple items.
 *
 * @param unpack Generic unpacker to be used for unpacking.
 * @param item Item where info about the unpacked item and its value is placed
 *   to.
 * @returns Boolean signaling if unpack was successful (non-invalid item was
 *   unpacked).
 */
[[gnu::nonnull]]
static inline bool cp_unpack(cp_unpack_t unpack, struct cpitem *item) {
	(*unpack)(unpack, item);
	return item->type != CPITEM_INVALID;
}

/*! Unpack item with generic unpacker and provide its type.
 *
 * This is variant of @ref cp_unpack() that instead of number of read bytes
 * returns type of the item read.
 *
 * @param unpack Generic unpacker to be used for unpacking.
 * @param item Item where info about the unpacked item and its value is placed
 *   to.
 * @returns Type of the unpacked item.
 */
[[gnu::nonnull]]
static inline enum cpitem_type cp_unpack_type(
	cp_unpack_t unpack, struct cpitem *item) {
	(*unpack)(unpack, item);
	return item->type;
}

/*! Drop any unread blocks from current unpacked item. The effect is that next
 * unpack will unpack a new item instead of next block of the current one.
 *
 * This is here for strings and blobs and only skips all unread bytes from them,
 * it won't continue to the next item. This allows you to just skip any bytes of
 * data you do not care about and go to the next item.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 */
[[gnu::nonnull]]
void cp_unpack_drop1(cp_unpack_t unpack, struct cpitem *item);

/*! Drop currently unpacked item. The effect is that the next unpack will
 * unpack new item after container end. This uses @ref cp_unpack_drop1 for
 * items that are not containers.
 *
 * The usage is when you are not interested in already unpacked item. It is an
 * alternative for already unpacked item to @ref cp_unpack_skip.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 */
[[gnu::nonnull]]
void cp_unpack_drop(cp_unpack_t unpack, struct cpitem *item);

/*! Instead of getting next item this skips it.
 *
 * The difference between getting and not using the item is that this skips the
 * whole item. That means the whole list or map as well as whole string or blob.
 * This includes multiple calls to the `cp_unpack`.
 *
 * This also fist drops any unfinished string or blob with `cp_unpack_drop1` to
 * for sure finish the previous item.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the last `cp_unpack` call. This is required
 * because there might be unfinished string or blob we need to know about.
 */
[[gnu::nonnull]]
void cp_unpack_skip(cp_unpack_t unpack, struct cpitem *item);

/*! Read until the currently opened container is finished.
 *
 * This is same function as `cp_unpack_skip` with difference that it stops not
 * after end of the next item, but on container end for which there was no
 * container open. The effect is that opened container is skipped.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the last `cp_unpack` call. This is required
 * because there might be unfinished string or blob we need to know about.
 * @param depth How many containers we should finish. To finish one pass `1`.
 * To close two containers pass `2` and so on. The `0` provides the same
 * functionality as `cp_unpack_skip`.
 */
[[gnu::nonnull]]
void cp_unpack_finish(cp_unpack_t unpack, struct cpitem *item, unsigned depth);


/*! Unpack integer and place it to the destination.
 *
 * This combines @ref cp_unpack with @ref cpitem_extract_int.
 *
 * @param UNPACK Generic unpacker to be used for unpacking.
 * @param ITEM Item where info about the unpacked item and its value is placed
 *   to. You can use it to identify the real type or error in case of failure.
 * @param DEST destination integer variable (not pointer, the variable
 *   directly).
 * @returns Boolean signaling if both unpack was successful and value fit the
 *   destination. The real issue can be deduced from **ITEM**.
 */
#define cp_unpack_int(UNPACK, ITEM, DEST) \
	({ \
		struct cpitem *___item = ITEM; \
		cp_unpack(UNPACK, ___item); \
		cpitem_extract_int(___item, DEST); \
	})

/*! Unpack unsigned integer and place it to the destination.
 *
 * This combines @ref cp_unpack with @ref cpitem_extract_uint.
 *
 * @param UNPACK Generic unpacker to be used for unpacking.
 * @param ITEM Item where info about the unpacked item and its value is placed
 *   to. You can use it to identify the real type or error in case of failure.
 * @param DEST destination integer variable (not pointer, the variable
 *   directly).
 * @returns Boolean signaling if both unpack was successful and value fit the
 *   destination. The real issue can be deduced from **ITEM**.
 */
#define cp_unpack_uint(UNPACK, ITEM, DEST) \
	({ \
		struct cpitem *___item = ITEM; \
		cp_unpack(UNPACK, ___item); \
		cpitem_extract_uint(___item, DEST); \
	})

/*! Unpack boolean and place it to the destination.
 *
 * This combines @ref cp_unpack with @ref cpitem_extract_bool.
 *
 * @param UNPACK: Generic unpacker to be used for unpacking.
 * @param ITEM: Item where info about the unpacked item and its value is placed
 *   to. You can use it to identify the real type or error in case of failure.
 * @param DEST: destination boolean variable (not pointer, the variable
 *   directly).
 * @returns Boolean signaling if both unpack was successful and value fit the
 *   destination. The real issue can be deduced from **ITEM**.
 */
#define cp_unpack_bool(UNPACK, ITEM, DEST) \
	({ \
		struct cpitem *___item = ITEM; \
		cp_unpack(UNPACK, ___item); \
		cpitem_extract_bool(___item, DEST); \
	})

/*! Unpack @ref cpdatetime and place it to the destination.
 *
 * This combines @ref cp_unpack with @ref cpitem_extract_datetime.
 *
 * @param UNPACK: Generic unpacker to be used for unpacking.
 * @param ITEM: Item where info about the unpacked item and its value is placed
 *   to. You can use it to identify the real type or error in case of failure.
 * @param DEST: destination @ref cpdatetime variable (not pointer, the variable
 *   directly).
 * @returns Boolean signaling if both unpack was successful and value fit the
 *   destination. The real issue can be deduced from **ITEM**.
 */
#define cp_unpack_datetime(UNPACK, ITEM, DEST) \
	({ \
		struct cpitem *___item = ITEM; \
		cp_unpack(UNPACK, ___item); \
		cpitem_extract_datetime(___item, DEST); \
	})

/*! Unpack a single byte from string.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns Unpacked byte or `-1` in case of an unpack error.
 */
[[gnu::nonnull]]
static inline int cp_unpack_getc(cp_unpack_t unpack, struct cpitem *item) {
	uint8_t res = 0;
	item->buf = &res;
	item->bufsiz = 1;
	cp_unpack(unpack, item);
	item->bufsiz = 0;
	if (item->type != CPITEM_STRING)
		return -1;
	return res;
}

/*! Copy string to the provided buffer.
 *
 * Be aware that compared to the `strncpy` this has a different return!
 *
 * Warning: Null byte is appended only if it fits. You can check for that by
 * comparing returned value and size of the destination.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param dest Destination buffer where unpacked characters are placed to.
 * @param n Maximum number of bytes to be used in `dest`.
 * @returns number of bytes written to `dest` or `-1` in case of unpack error.
 */
[[gnu::nonnull(1, 2)]]
ssize_t cp_unpack_strncpy(
	cp_unpack_t unpack, struct cpitem *item, char *dest, size_t n);

/*! Copy string to malloc allocated buffer and return it.
 *
 * This unpacks the whole string and returns it.
 *
 * The error can be detected by checking: `item->type == CPITEM_STRING`.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns malloc allocated null terminated string.
 */
[[gnu::nonnull]]
char *cp_unpack_strdup(cp_unpack_t unpack, struct cpitem *item);

/*! Copy string of at most given length to malloc allocated buffer and return
 * it.
 *
 * This unpacks the string up to the given length and returns it. It always
 * appends '\0'.
 *
 * The error can be detected by checking: `item->type == CPITEM_STRING`.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param len Maximum number of bytes to be copied.
 * @returns malloc allocated null terminated string.
 */
[[gnu::nonnull]]
char *cp_unpack_strndup(cp_unpack_t unpack, struct cpitem *item, size_t len);

/*! Copy string to obstack allocated buffer and return it.
 *
 * This is same as @ref cp_unpack_strdup except it uses obstack to allocated the
 * required space.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param obstack Obstack used to allocate the space needed for the string
 * object.
 * @returns pointer to the allocated string object.
 */
[[gnu::nonnull]]
char *cp_unpack_strdupo(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);

/*! Grow obstack object with unpacked string.
 *
 * This is same as @ref cp_unpack_strdupo except that allocation is not
 * finished.
 *
 * The string is always terminated with `'\0'` but you can always remove it with
 * `obstack_blank(obstack, -1)`.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param obstack Obstack used to push string to.
 * @returns `true` if unpack was successful or `false` in case it wasn't. It is
 *   possible that there is something pushed to the obstack even when `false` is
 *   returned.
 */
[[gnu::nonnull]]
bool cp_unpack_strdupog(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);

/*! Copy string up to given length to obstack allocated buffer and return it.
 *
 * This is same as @ref cp_unpack_strndup except it uses obstack to allocated
 * the required space.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param len Maximum number of bytes to be copied.
 * @param obstack Obstack used to allocate the space needed for the string
 * object.
 * @returns pointer to the allocated string object.
 */
[[gnu::nonnull]]
char *cp_unpack_strndupo(cp_unpack_t unpack, struct cpitem *item, size_t len,
	struct obstack *obstack);

/*! Grow obstack object with unpacked string with up to the given number of
 * bytes.
 *
 * This is same as @ref cp_unpack_strndupo except that allocation is not
 * finished.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param len Maximum number of bytes to be copied.
 * @param obstack Obstack used to allocate the space needed for the string
 * object.
 * @returns `true` if unpack was successful or `false` in case it wasn't. It
 * is possible that there is something pushed to the obstack even when
 * `false` is returned.
 */
[[gnu::nonnull]]
bool cp_unpack_strndupog(cp_unpack_t unpack, struct cpitem *item, size_t len,
	struct obstack *obstack);

/*! Copy blob to the provided buffer.
 *
 * Be aware that compared to the `memcpy` this has a different return!
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param dest Destination buffer where unpacked bytes are placed to.
 * @param siz Maximum number of bytes to be used in `dest`.
 * @returns number of bytes written to `dest` or `-1` in case of unpack
 * error.
 */
[[gnu::nonnull(1, 2)]]
ssize_t cp_unpack_memcpy(
	cp_unpack_t unpack, struct cpitem *item, uint8_t *dest, size_t siz);

/*! Copy blob to malloc allocated buffer and provide it.
 *
 * This unpacks the whole blob and returns it.
 *
 * The error can be detected by checking: `item->type == CPITEM_BLOB`.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data Pointer where pointer to the data would be placed.
 * @param siz Pointer where number of valid data bytes were unpacked.
 */
[[gnu::nonnull]]
void cp_unpack_memdup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **data, size_t *siz);

/*! Copy blob up to given number of bytes to malloc allocated buffer and
 * provide it.
 *
 * This unpacks the blob up to the given size and returns it.
 *
 * The error can be detected by checking: `item->type == CPITEM_BLOB`.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param data Pointer where pointer to the data would be placed.
 * @param siz Pointer to maximum number of bytes to be copied that is
 * updated with number of valid bytes actually copied over.
 */
[[gnu::nonnull]]
void cp_unpack_memndup(
	cp_unpack_t unpack, struct cpitem *item, uint8_t **data, size_t *siz);

/*! Copy blob to obstack allocated buffer and return it.
 *
 * This is same as @ref cp_unpack_memdup except it uses obstack to allocated
 * the required space.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param buf Pointer where pointer to the data would be placed.
 * @param siz Pointer where number of valid data bytes were unpacked.
 * @param obstack Obstack used to allocate the space needed for the blob
 * object.
 */
[[gnu::nonnull]]
void cp_unpack_memdupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack);

/*! Grow obstack object with unpacked bytes.
 *
 * This is same as @ref cp_unpack_memdupo except that allocation is not
 * finished and thus you can continue with object growing.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param obstack Obstack used to allocate the space needed for the blob
 * object.
 * @returns `true` if unpack was successful or `false` in case it wasn't. It is
 *   possible that there is something pushed to the obstack even when `false` is
 *   returned.
 */
[[gnu::nonnull]]
bool cp_unpack_memdupog(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack);

/*! Copy blob up to given number of bytes to obstack allocated buffer and
 * return it.
 *
 * This is same as @ref cp_unpack_memndup except it uses obstack to
 * allocated the required space.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param buf Pointer where pointer to the data would be placed.
 * @param siz Pointer to maximum number of bytes to be copied that is
 * updated with number of valid bytes actually copied over.
 * @param obstack Obstack used to allocate the space needed for the blob
 * object.
 */
[[gnu::nonnull]]
void cp_unpack_memndupo(cp_unpack_t unpack, struct cpitem *item, uint8_t **buf,
	size_t *siz, struct obstack *obstack);

/*! Grow obstack object with unpacked bytes of up to given number.
 *
 * This is same as @ref cp_unpack_memndupo except that allocations is not
 * finished and thus you can continue with object growing.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @param siz Maximum number of bytes to be copied.
 * @param obstack Obstack used to allocate the space needed for the blob
 * object.
 * @returns `true` if unpack was successful or `false` in case it wasn't. It is
 *   possible that there is something pushed to the obstack even when `false` is
 *   returned.
 */
[[gnu::nonnull]]
bool cp_unpack_memndupog(cp_unpack_t unpack, struct cpitem *item, size_t siz,
	struct obstack *obstack);

/*! Helper macro that iterates over complete items.
 *
 * This is same as @ref cp_unpack_finish expect that item is always available to
 * you.
 *
 * You must set @ref cpitem.bufsiz to also immediatelly copy or drop strings and
 * blobs, or you must extract them on your own. The unpacking will get stuck on
 * strings and blobs if you do not do this!
 *
 * @param UNPACK Generic unpacker.
 * @param ITEM Pointer to the @ref cpitem that was used to unpack last item.
 * @param DEPTH How many containers we should got from the currently unpacked
 * one.
 */
#define for_cp_unpack_item(UNPACK, ITEM, DEPTH) \
	for (int __depth = ({ \
			 cp_unpack((UNPACK), (ITEM)); \
			 DEPTH; \
		 }); \
		 __depth >= 0 && (ITEM)->type != CPITEM_INVALID; ({ \
			 switch ((ITEM)->type) { \
				 case CPITEM_BLOB: \
				 case CPITEM_STRING: \
					 if (item->as.Blob.flags & CPBI_F_FIRST) \
						 __depth++; \
					 if (item->as.Blob.flags & CPBI_F_LAST) \
						 __depth--; \
					 break; \
				 case CPITEM_LIST: \
				 case CPITEM_MAP: \
				 case CPITEM_IMAP: \
				 case CPITEM_META: \
					 __depth++; \
					 break; \
				 case CPITEM_CONTAINER_END: \
					 __depth--; \
					 break; \
				 case CPITEM_INVALID: \
					 __depth = -1; \
				 default: \
					 break; \
			 } \
			 if (__depth > 0) \
				 cp_unpack((UNPACK), (ITEM)); \
			 else \
				 __depth = -1; \
		 }))

/*! Helper macro for unpacking lists.
 *
 * Lists are sequence of items starting with @ref CPITEM_LIST item and ending
 * with @ref CPITEM_CONTAINER_END. This provides loop that iterates over items
 * in the list.
 *
 * You need to call this when you unpack @ref CPITEM_LIST and thus this does
 * not check if you are calling it in list or not. It immediately starts
 * unpacking next item and thus first item in the list.
 *
 * @param UNPACK Generic unpacker.
 * @param ITEM Pointer to the @ref cpitem that was used to unpack last item.
 */
#define for_cp_unpack_list(UNPACK, ITEM) \
	while (({ \
		cp_unpack((UNPACK), (ITEM)); \
		(ITEM)->type != CPITEM_INVALID && (ITEM)->type != CPITEM_CONTAINER_END; \
	}))

/*! Helper macro for unpacking lists with items counter.
 *
 * This is variant of @ref for_cp_unpack_list with items counter from zero.
 *
 * It is expected the currently unpacked item is the List beginning.
 *
 * @param UNPACK Generic unpacker.
 * @param ITEM Pointer to the @ref cpitem that was used to unpack last item.
 * @param CNT Counter variable name. The variable is defined in the for loop.
 */
#define for_cp_unpack_ilist(UNPACK, ITEM, CNT) \
	for (unsigned(CNT) = 0; ({ \
			 struct cpitem *___item = ITEM; \
			 cp_unpack(UNPACK, ___item); \
			 ___item->type != CPITEM_INVALID && \
				 ___item->type != CPITEM_CONTAINER_END; \
		 }); \
		 (CNT)++)

/*! Helper macro for unpacking maps.
 *
 * This provides loop that unpacks and validates key. The key value is copied
 * to temporally buffer named `KEY` of `SIZ + 2` size. The additional byte is
 * reserved for additional character to recognize longer strings and null byte
 * and thus `SIZ` is only a maximal length of the string.
 *
 * Make sure that you unpack value to not desynchronize the key value pairs
 * order. You can skip or drop unneeded items (@ref cp_unpack_skip).
 *
 * It is expected the currently unpacked item is the Map beginning.
 *
 * @param UNPACK Generic unpacker.
 * @param ITEM Pointer to the @ref cpitem that was used to unpack last item.
 * @param KEY Name of the variable for the temporally string buffer. It is
 *   defined in the loop.
 * @param SIZ Maximal length of the key that is expected.
 */
#define for_cp_unpack_map(UNPACK, ITEM, KEY, SIZ) \
	for (char(KEY)[SIZ + 2]; ({ \
			 (ITEM)->chr = KEY; \
			 (ITEM)->bufsiz = SIZ + 1; \
			 cp_unpack((UNPACK), (ITEM)); \
			 (ITEM)->bufsiz = 0; \
			 if ((ITEM)->type == CPITEM_STRING) \
				 (KEY)[(ITEM)->as.String.len] = '\0'; \
			 (ITEM)->type == CPITEM_STRING; \
		 });)

/*! Helper macro for unpacking integer maps.
 *
 * It goes through the keys and provides them in `IKEY` variable. Make sure that
 * you unpack value to not desynchronize the key-value pairing. You can skip
 * unneeded values with @ref cp_unpack_skip.
 *
 * It is expected the currently unpacked item is the IMap beginning.
 *
 * @param UNPACK Generic unpacker.
 * @param ITEM Pointer to the @ref cpitem that was used to unpack last item.
 * @param IKEY Variable name for integer key. It is defined in the for loop.
 */
#define for_cp_unpack_imap(UNPACK, ITEM, IKEY) \
	for (long long IKEY; cp_unpack_int((UNPACK), (ITEM), IKEY);)

/*! Open string or blob for reading using stream.
 *
 * This allows you to use `fscanf` and other functions to deal with strings and
 * blobs.
 *
 * To detect if you are dealing with string or blob you need to use `item->type`
 * after you call this function.
 *
 * @param unpack Unpack handle.
 * @param item Item used for the `cp_unpack` calls and was used in the last
 * one.
 * @returns File object or `NULL` in case of unpack error.
 */
[[gnu::nonnull]]
FILE *cp_unpack_fopen(cp_unpack_t unpack, struct cpitem *item);


#endif
