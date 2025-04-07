#ifndef _SHVC_OPTS_H_
#define _SHVC_OPTS_H_
#include <stdbool.h>

struct conf {
	const char *url;
	const char *path;
	const char *method;
	const char *param;
	const char *userid;
	bool stdin_param;
	unsigned verbose;
	unsigned timeout;
};

/* Parse arguments. */
[[gnu::nonnull]]
void parse_opts(int argc, char **argv, struct conf *conf);

#endif
