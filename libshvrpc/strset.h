#ifndef SHV_HASHSET_H
#define SHV_HASHSET_H

#include <stdbool.h>
#include <stddef.h>


struct strset {
	struct strset_item {
		unsigned hash;
		const char *str;
		bool dyn;
	} *items;
	size_t cnt, siz;
};

[[gnu::nonnull]]
void shv_strset_free(struct strset *set);

/* Add item. This item is duplicated to be kept. */
[[gnu::nonnull]]
bool shv_strset_add(struct strset *set, const char *str);

/* Add item but it is considered to be constant and thus kept for the duration
 * of execution the same.
 */
[[gnu::nonnull]]
bool shv_strset_add_const(struct strset *set, const char *str);

/* Add item that is freed when hash is freed. */
[[gnu::nonnull]]
bool shv_strset_add_dyn(struct strset *set, char *str);



#endif
