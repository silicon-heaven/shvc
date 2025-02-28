#ifndef _SHVCBROKER_OPTS_H_
#define _SHVCBROKER_OPTS_H_
#include <stdbool.h>

struct opts {
	const char *config;
	unsigned verbose;
};

/* Parse arguments. */
[[gnu::nonnull]]
void parse_opts(int argc, char **argv, struct opts *opts);

#endif
