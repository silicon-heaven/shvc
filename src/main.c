/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <foo.h>
#include "config.h"


int main(int argc, char **argv) {
	struct config conf;
	load_config(&conf, argc, argv);

	FILE *f = stdin;
	bool close = false;
	if (conf.source_file != NULL && strcmp(conf.source_file, "-")) {
		close = true;
		f = fopen(conf.source_file, "r");
		if (f == NULL) {
			fprintf(stderr, "Can't open input file '%s': %s\n",
				conf.source_file, strerror(errno));
			return 1;
		}
	}

	unsigned cnt = count_foo(f);
	printf("%u\n", cnt);

	if (close)
		fclose(f);
	return 0;
}
