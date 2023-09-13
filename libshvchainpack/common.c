#include "common.h"
#include <sys/param.h>

bool common_unpack(size_t *res, FILE *f, struct cpitem *item) {
	switch (item->type) {
		case CPITEM_INVALID:
			/* Do nothing in case previous item reported an error */
			return item->as.Error != CPERR_NONE;
		case CPITEM_RAW:
			break;
		default:
			return false;
	}

	/* Handle raw read */
	if (item->buf) {
		item->as.Blob.len = fread(item->buf, 1, item->bufsiz, f);
		*res += item->as.Blob.len;
	} else {
		size_t siz = item->bufsiz;
		uint8_t buf[MIN(BUFSIZ, item->bufsiz)];
		size_t i;
		do {
			i = fread(buf, 1, MIN(BUFSIZ, siz), f);
			siz -= i;
			*res += i;
		} while (i != 0);
	}
	return true;
}

bool common_pack(size_t *res, FILE *f, const struct cpitem *item) {
	switch (item->type) {
		case CPITEM_INVALID:
			/* Do nothing for invalid item */
			return true;
		case CPITEM_RAW:
			break;
		default:
			return false;
	}

	/* Handle raw write */
	if (item->as.Blob.len == 0 || fwrite(item->buf, item->as.Blob.len, 1, f) == 1)
		*res = item->as.Blob.len;
	else
		*res = -1;
	return true;
}
