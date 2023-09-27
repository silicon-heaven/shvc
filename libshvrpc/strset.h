#ifndef SHV_HASHSET_H
#define SHV_HASHSET_H

#include <stdbool.h>
#include <stddef.h>


struct strset {
	struct strset_item {
		unsigned hash;
		const char *str;
		bool dyn;
	} * items;
	size_t cnt, siz;
};

void shv_strset_free(struct strset *set) __attribute__((nonnull));

/* Add item. This item is duplicated to be kept. */
void shv_strset_add(struct strset *set, const char *str) __attribute__((nonnull));

/* Add item but it is considered to be constant and thus kept for the duration
 * of execution the same.
 */
void shv_strset_add_const(struct strset *set, const char *str)
	__attribute__((nonnull));

/* Add item that is freed when hash is freed. */
void shv_strset_add_dyn(struct strset *set, char *str) __attribute__((nonnull));



#endif
