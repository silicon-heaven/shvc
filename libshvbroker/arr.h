#ifndef SHVBROKER_ARR_H
#define SHVBROKER_ARR_H

#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>


#define ARR(TYPE, NAME) \
	TYPE *NAME; \
	size_t NAME##_cnt

#define ARR_INIT(VAR) \
	VAR = NULL; \
	VAR##_cnt = 0

#define ARR_RESET(VAR) \
	free(VAR); \
	ARR_INIT(VAR)

#define ARR_ADD(VAR) \
	({ \
		VAR = realloc(VAR, (VAR##_cnt + 1) * sizeof *VAR); \
		assert(VAR); \
		VAR + VAR##_cnt++; \
	})

/* Remove specific item from array */
#define ARR_DEL(VAR, PTR) \
	do { \
		VAR##_cnt -= 1; \
		if (VAR##_cnt == 0) { \
			free(VAR); \
			VAR = NULL; \
		} else { \
			memmove((PTR), (PTR) + 1, (VAR##_cnt - ((PTR) - VAR)) * sizeof *VAR); \
			VAR = realloc(VAR, VAR##_cnt * sizeof *VAR); \
		} \
	} while (false)

/* Remove sequence from the array start. */
#define ARR_DROP(VAR, CNT) \
	do { \
		if (VAR##_cnt <= (CNT)) { \
			free(VAR); \
			VAR = NULL; \
			VAR##_cnt = 0; \
		} else if (VAR##_cnt != (CNT)) { \
			memmove(VAR, VAR + (CNT), (VAR##_cnt - (CNT)) * sizeof *VAR); \
			VAR##_cnt -= CNT; \
			VAR = realloc(VAR, VAR##_cnt * sizeof *VAR); \
		} \
	} while (false)

#define ARR_QSORT(VAR, FUNC) qsort(VAR, VAR##_cnt, sizeof *VAR, FUNC);

#define ARR_BSEARCH(REF, VAR, FUNC) \
	bsearch(REF, VAR, VAR##_cnt, sizeof *VAR, FUNC);

#endif
