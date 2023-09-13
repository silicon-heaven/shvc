#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


static void print_usage(const char *argv0) {
	fprintf(stderr, "%s [-Vh] [-c FILE]\n", argv0);
}

static void print_help(const char *argv0) {
	print_usage(argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "  -c FILE  Configuration file\n");
	fprintf(stderr, "  -V       Print version and exit\n");
	fprintf(stderr, "  -h       Print this help text\n");
}

static void parse_opts(const char **confpath, int argc, char **argv) {
	int c;
	while ((c = getopt(argc, argv, "c:Vh")) != -1) {
		switch (c) {
			case 'c':
				*confpath = argv[optind];
				break;
			case 'V':
				printf("%s " PROJECT_VERSION, argv[0]);
				exit(0);
			case 'h':
				print_help(argv[0]);
				exit(0);
			default:
				print_usage(argv[0]);
				fprintf(stderr, "Invalid option: -%c\n", c);
				exit(2);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Invalid argument: %s\n", argv[optind + 1]);
		exit(2);
	}
}


void load_config(int argc, char **argv) {
	const char *confpath = NULL;
	parse_opts(&confpath, argc, argv);
	// TODO load config
}
