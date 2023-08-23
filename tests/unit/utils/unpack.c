#include "unpack.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

struct unpack {
	union {
		struct cp_unpack_chainpack chainpack;
		struct cp_unpack_cpon cpon;
	};
	bool is_cpon;
	FILE *f;
};

cp_unpack_t unpack_chainpack(struct bdata *b) {
	struct unpack *u = malloc(sizeof *u);
	u->f = fmemopen((void *)b->v, b->len, "r");
	u->is_cpon = false;
	return cp_unpack_chainpack_init(&u->chainpack, u->f);
}

cp_unpack_t unpack_cpon(const char *str) {
	struct unpack *u = malloc(sizeof *u);
	u->f = fmemopen((void *)str, strlen(str), "r");
	u->is_cpon = true;
	return cp_unpack_cpon_init(&u->cpon, u->f);
}

void unpack_free(cp_unpack_t unpack) {
	struct unpack *u = (struct unpack *)unpack;
	if (u->is_cpon)
		free(u->cpon.state.ctx);
	fclose(u->f);
	free(u);
}
