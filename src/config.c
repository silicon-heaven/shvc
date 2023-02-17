/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#include "config.h"
#include <unistd.h>
#include <argp.h>

static struct config conf = {
	.source_file = NULL,
};

static error_t parse_opt(int key, char *arg, struct argp_state *state);

const static struct argp_option argp_options[] = {{NULL}};

const char *argp_program_version = PROJECT_VERSION;
const char *argp_program_bug_address = "bug@example.com";
const static struct argp argp_parser = {
	.options = argp_options,
	.parser = parse_opt,
	.doc = "Counts number of lines starting with \"foo\"",
	.args_doc = "[FILE]",
};


static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	switch (key) {
		case ARGP_KEY_ARG:
			if (conf.source_file == NULL) {
				conf.source_file = arg;
				break;
			}
			__attribute__((fallthrough));
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}


struct config *load_config(int argc, char **argv) {
	argp_parse(&argp_parser, argc, argv, 0, NULL, NULL);
	return &conf;
}
