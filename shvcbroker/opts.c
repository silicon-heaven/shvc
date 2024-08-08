#include "opts.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>


static void print_usage(const char *argv0) {
	fprintf(stderr, "%s [-vqdVh] [-c FILE]\n", argv0);
}

static void print_help(const char *argv0) {
	print_usage(argv0);
	fprintf(stderr, "SHV RPC Broker.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "  -c FILE  Path to the configuration file\n");
	fprintf(stderr, "  -v       Increase logging level of the communication\n");
	fprintf(stderr, "  -q       Decrease logging level of the communication\n");
	fprintf(stderr, "  -d       Set maximul logging level of the communication\n");
	fprintf(stderr, "  -V       Print SHVC version and exit\n");
	fprintf(stderr, "  -h       Print this help text\n");
}

void parse_opts(int argc, char **argv, struct opts *opts) {
	*opts = (struct opts){
		.config = NULL,
		.verbose = 0,
	};

	int c;
	while ((c = getopt(argc, argv, "c:vqdVh")) != -1) {
		switch (c) {
			case 'c':
				opts->config = optarg;
				break;
			case 'v':
				if (opts->verbose < UINT_MAX)
					opts->verbose++;
				break;
			case 'q':
				if (opts->verbose > 0)
					opts->verbose--;
				break;
			case 'd':
				opts->verbose = UINT_MAX;
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
				exit(-1);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
		exit(-1);
	}
	// TODO locate the opts file in some predefined locations
}
