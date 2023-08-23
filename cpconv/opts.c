#include "opts.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


static void print_usage(const char *argv0) {
	fprintf(stderr, "%s [-punVh] [INPUT] [OUTPUT]\n", argv0);
}

static void print_help(const char *argv0) {
	print_usage(argv0);
	fprintf(stderr, "Convert between Chainpack and human readable Cpon.\n");
	fprintf(stderr,
		"The conversion direction is can be autodetected but it is highly "
		"advised to specify the direction if possible.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "  -p       Input is Cpon and output is Chainpack (Pack)\n");
	fprintf(stderr, "  -u       Input is Chainpack and output is Cpon (Unpack)\n");
	fprintf(stderr, "  -n       Pretty print CPON instead of single line\n");
	fprintf(stderr, "  -V       Print version and exit\n");
	fprintf(stderr, "  -h       Print this help text\n");
}

static void opset(struct conf *conf, enum operation op) {
	if (conf->op != OP_DETECT && conf->op != op) {
		fprintf(stderr, "You can use only -p or -u not both at the same time!");
		exit(2);
	}
	conf->op = op;
}

void parse_opts(int argc, char **argv, struct conf *conf) {
	*conf = (struct conf){
		.input = NULL,
		.output = NULL,
		.op = OP_DETECT,
		.pretty = false,
	};

	int c;
	while ((c = getopt(argc, argv, "punVh")) != -1) {
		switch (c) {
			case 'p':
				opset(conf, OP_PACK);
				break;
			case 'u':
				opset(conf, OP_UNPACK);
				break;
			case 'n':
				conf->pretty = true;
				break;
			case 'V':
				printf("%s " PROJECT_VERSION "\n", argv[0]);
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
	if (optind < argc)
		conf->input = argv[++optind];
	if (optind < argc)
		conf->output = argv[++optind];
	if (optind < argc) {
		fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
		exit(2);
	}
}
