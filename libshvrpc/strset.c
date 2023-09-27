#include "strset.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static unsigned strhash(const char *str) {
	unsigned res = 0;
	while (*str != '\0')
		res = (res << 8 | res >> ((sizeof(unsigned) - 1) * 8)) ^ *(str++);
	return res;
}

static size_t strpos(const struct strset *set, int hsh) {
	if (set->cnt == 0)
		return 0;
	size_t min = 0;
	size_t max = set->cnt - 1;
	while (min != max) {
		size_t i = (min + max + 1) / 2;
		if (set->items[i].hash > hsh)
			max = i - 1;
		else
			min = i;
	}
	if (set->items[min].hash >= hsh)
		return min;
	return min + 1;
}

static void add(struct strset *set, const char *str, bool dyn) {
	unsigned hsh = strhash(str);
	size_t pos = strpos(set, hsh);
	if (pos < set->cnt && set->items[pos].hash == hsh &&
		!strcmp(set->items[pos].str, str))
		return; /* Already in the set */

	if (set->cnt >= set->siz) {
		set->siz = set->siz * 2 ?: 2;
		set->items = realloc(set->items, set->siz * sizeof(*set->items));
	}
	if (pos < set->cnt)
		memmove(set->items + pos + 1, set->items + pos,
			(set->cnt - pos) * sizeof *set->items);
	set->items[pos] = (struct strset_item){.hash = hsh, .str = str, .dyn = dyn};
	set->cnt++;
}


void shv_strset_free(struct strset *set) {
	for (size_t i = 0; i < set->cnt; i++)
		if (set->items[i].dyn)
			free((void *)set->items[i].str);
	free(set->items);
	set->cnt = 0;
	set->siz = 0;
}

void shv_strset_add(struct strset *set, const char *str) {
	add(set, strdup(str), true);
}

void shv_strset_add_const(struct strset *set, const char *str) {
	add(set, str, false);
}

void shv_strset_add_dyn(struct strset *set, char *str) {
	add(set, str, true);
}
