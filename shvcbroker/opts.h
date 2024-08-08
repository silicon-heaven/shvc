#ifndef _SHVCBROKER_OPTS_H_
#define _SHVCBROKER_OPTS_H_
#include <stdbool.h>

struct opts {
	const char *config;
	unsigned verbose;
};

/* Parse arguments. */
void parse_opts(int argc, char **argv, struct opts *opts) __attribute__((nonnull));

#endif
