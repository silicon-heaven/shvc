/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#include "config.h"
#include <unistd.h>
#ifdef __NuttX__
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#else
#include <argp.h>
#endif

/* NOTE: The two ways to parse arguments are provided here. NuttX does not have
 * argp and thus we can't use it. We should use getopt instead but contrary to
 * this file you should choose only one of the argument parsing tools. The rule
 * is: if you plan on deploying on NuttX then you getopt, otherwise argp.
 */
#ifdef __NuttX__

static void print_usage(const char *argv0) {
	fprintf(stderr, "%s [-hV] [FILE]\n", argv0);
}

static void print_help(const char *argv0) {
	print_usage(argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "  -V  Print application version\n");
	fprintf(stderr, "  -h  Print this help text\n");
}

static void parse_opts(struct config *conf, int argc, char **argv) {
	int c;
	while ((c = getopt(argc, argv, "h")) != -1) {
		switch (c) {
			case 'h':
				print_help(argv[0]);
				exit(0);
			case 'V':
				puts(PROJECT_VERSION);
				exit(0);
			default:
				print_usage(argv[0]);
				fprintf(stderr, "Invalid option: -%c\n", c);
				exit(2);
		}
	}
	if (optind < argc) {
		if (optind + 1 == argc)
			conf->source_file = argv[optind];
		else {
			fprintf(stderr, "Invalid argument: %s\n", argv[optind + 1]);
			exit(2);
		}
	}
}

#else

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
	struct config *conf = state->input;
	switch (key) {
		case ARGP_KEY_ARG:
			if (conf->source_file == NULL) {
				conf->source_file = arg;
				break;
			}
			__attribute__((fallthrough));
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#endif


void load_config(struct config *conf, int argc, char **argv) {
	*conf = (struct config){
		.source_file = NULL,
	};
#ifdef __NuttX__
	parse_opts(conf, argc, argv);
#else
	argp_parse(&argp_parser, argc, argv, 0, NULL, conf);
#endif
}
