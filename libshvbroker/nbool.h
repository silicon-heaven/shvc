#ifndef SHVBROKER_NBOOL_H
#define SHVBROKER_NBOOL_H

#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define NBOOL_B (sizeof(unsigned))
#define NBOOL_N (CHAR_BIT * NBOOL_B)

typedef struct nbool {
	unsigned cnt;
	unsigned vals[];
} *nbool_t;


static bool nbool_valid_index(nbool_t v, unsigned i) {
	return v != NULL && i < (v->cnt * NBOOL_N);
}

__attribute__((nonnull)) static inline void __nbool_incease_size(
	nbool_t *v, size_t cnt) {
	size_t pcnt = *v ? (*v)->cnt : 0;
	*v = (nbool_t)realloc(*v, sizeof **v + cnt * NBOOL_B);
	(*v)->cnt = cnt;
	memset((*v)->vals + pcnt, 0, (cnt - pcnt) * NBOOL_B);
}

__attribute__((nonnull)) static inline void nbool_set(nbool_t *v, unsigned i) {
	if (!nbool_valid_index(*v, i))
		__nbool_incease_size(v, i / NBOOL_N + 1);
	(*v)->vals[i / NBOOL_N] |= 1 << (i % NBOOL_N);
}

__attribute__((nonnull)) static inline void nbool_clear(nbool_t *v, unsigned i) {
	if (!nbool_valid_index(*v, i))
		return;
	(*v)->vals[i / NBOOL_N] &= ~(1 << (i % NBOOL_N));
	unsigned ni = (*v)->cnt;
	while (ni > 0 && (*v)->vals[ni - 1] == 0)
		ni--;
	if (ni == 0) {
		free(*v);
		*v = NULL;
	} else if (ni < (*v)->cnt) {
		(*v)->cnt = ni;
		*v = (nbool_t)realloc(*v, sizeof **v + (*v)->cnt * NBOOL_B);
	}
}

__attribute((nonnull(1))) static inline void nbool_or(nbool_t *v, const nbool_t o) {
	if (o == NULL)
		return;
	if (*v == NULL || (*v)->cnt < o->cnt)
		__nbool_incease_size(v, o->cnt);
	for (size_t i = 0; i < o->cnt; i++)
		(*v)->vals[i] |= o->vals[i];
}

static inline bool nbool(nbool_t v, unsigned i) {
	if (!nbool_valid_index(v, i))
		return false;
	return (bool)(v->vals[i / NBOOL_N] & (1 << (i % NBOOL_N)));
}

#define for_nbool(V, VAR) \
	for (unsigned VAR = 0; (V) && VAR < (V->cnt * NBOOL_N); VAR++) \
		if (nbool((V), VAR))

#endif
