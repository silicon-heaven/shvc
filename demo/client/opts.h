#ifndef _DEMO_OPTS_H_
#define _DEMO_OPTS_H_
#include <stdbool.h>

#define CALL_TIMEOUT 300
#define TRACK_ID "4"

struct conf {
	const char *url;
	unsigned verbose;
};

/* Parse arguments. */
void parse_opts(int argc, char **argv, struct conf *conf) __attribute__((nonnull));

#endif
