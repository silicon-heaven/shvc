#ifndef _CPCONV_OPTS_H_
#define _CPCONV_OPTS_H_
#include <stdbool.h>

enum operation {
	OP_DETECT,
	OP_PACK,
	OP_UNPACK,
};

struct conf {
	const char *input, *output;
	enum operation op;
	bool pretty;
};

/* Parse arguments. */
void parse_opts(int argc, char **argv, struct conf *conf) __attribute__((nonnull));

#endif
