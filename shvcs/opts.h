#ifndef _SHVC_OPTS_H_
#define _SHVC_OPTS_H_
#include <stdbool.h>

struct conf {
	const char *url;
	unsigned verbose;
	char **ris;
};

/* Parse arguments. */
void parse_opts(int argc, char **argv, struct conf *conf) __attribute__((nonnull));

#endif
