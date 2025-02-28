#ifndef _SHVC_OPTS_H_
#define _SHVC_OPTS_H_
#include <stdbool.h>

struct conf {
	const char *url;
	const char *path;
	const char *method;
	const char *param;
	unsigned verbose;
	bool json;
};

/* Parse arguments. */
[[gnu::nonnull]]
void parse_opts(int argc, char **argv, struct conf *conf);

#endif
