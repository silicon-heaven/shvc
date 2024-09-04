#include "opts.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "shvc_config.h"

static char *ris_default[] = {"**:*:*", NULL};

static void print_usage(const char *argv0) {
	fprintf(stderr, "%s [-vqdVh] [-u URL] [RI]...\n", argv0);
}

static void print_help(const char *argv0) {
	print_usage(argv0);
	fprintf(stderr, "SHV RPC subscription client.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr,
		"  -u URL   Where to connect to (default " SHVC_DEFAULT_URL ")\n");
	fprintf(stderr, "  -v       Increase logging level of the communication\n");
	fprintf(stderr, "  -q       Decrease logging level of the communication\n");
	fprintf(stderr, "  -d       Set maximal logging level of the communication\n");
	fprintf(stderr, "  -V       Print SHVC version and exit\n");
	fprintf(stderr, "  -h       Print this help text\n");
}

void parse_opts(int argc, char **argv, struct conf *conf) {
	*conf = (struct conf){
		.url = SHVC_DEFAULT_URL,
		.verbose = 0,
		.ris = ris_default,
	};

	int c;
	while ((c = getopt(argc, argv, "u:vqdVh")) != -1) {
		switch (c) {
			case 'u':
				conf->url = optarg;
				break;
			case 'v':
				if (conf->verbose < UINT_MAX)
					conf->verbose++;
				break;
			case 'q':
				if (conf->verbose > 0)
					conf->verbose--;
				break;
			case 'd':
				conf->verbose = UINT_MAX;
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
		conf->ris = &argv[optind];
	// TODO validate RIs
}
