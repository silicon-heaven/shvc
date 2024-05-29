/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "match.gperf.h"

unsigned count_foo(FILE *f) {
	unsigned res = 0;
	char *line = NULL;
	size_t len = 0;
	while ((getline(&line, &len, f)) != -1) {
		char *colon = strchr(line, ':');
		if (colon == NULL)
			continue; // ignore lines without colon
		const struct gperf_match *gm = gperf_match(line, colon - line);
		if (gm != NULL && gm->matches)
			res++;
	}
	free(line);
	return res;
}
