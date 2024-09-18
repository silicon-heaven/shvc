#include "common.h"
#include <sys/param.h>

bool common_unpack(size_t *res, FILE *f, struct cpitem *item) {
	switch (item->type) {
		case CPITEM_INVALID:
			/* Do nothing in case previous item reported an error */
			return item->as.Error != CPERR_NONE;
		default:
			return false;
	}
}

bool common_pack(ssize_t *res, FILE *f, const struct cpitem *item) {
	if (f && ferror(f)) { /* No reason to write to file in error */
		*res = -1;
		return true;
	}
	switch (item->type) {
		case CPITEM_INVALID:
			/* Do nothing for invalid item */
			return true;
		default:
			return false;
	}
}
