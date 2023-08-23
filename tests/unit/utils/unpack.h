#ifndef UNPACK_H
#define UNPACK_H
#include <shv/cp_unpack.h>
#include "bdata.h"


cp_unpack_t unpack_chainpack(struct bdata *);

cp_unpack_t unpack_cpon(const char *str);

void unpack_free(cp_unpack_t);

#endif
