#include "opts.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include "shvc_config.h"

static const int default_timeout = 300;

static void print_usage(const char *argv0) {
	fprintf(stderr, "%s [-ivqdVh] [-u URL] [-t SEC] [PATH:METHOD] [PARAM]\n",
		argv0);
}

static void print_help(const char *argv0) {
	print_usage(argv0);
	fprintf(stderr, "SHV RPC client call.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr,
		"  -u URL   Where to connect to (default " SHVC_DEFAULT_URL ")\n");
	fprintf(stderr,
		"  -t SEC   Number of seconds before call is abandoned (default %d)\n",
		default_timeout);
	fprintf(stderr, "  -i       Read parameter from STDIN instead of argument\n");
	fprintf(stderr,
		"  -s UID   User ID to be passed to the request (default is no user ID)\n");
	fprintf(stderr, "  -v       Increase logging level of the communication\n");
	fprintf(stderr, "  -q       Decrease logging level of the communication\n");
	fprintf(stderr, "  -d       Set maximal logging level of the communication\n");
	fprintf(stderr, "  -V       Print SHVC version and exit\n");
	fprintf(stderr, "  -h       Print this help text\n");
}

void parse_opts(int argc, char **argv, struct conf *conf) {
	*conf = (struct conf){
		.url = SHVC_DEFAULT_URL,
		.path = ".app",
		.method = "ping",
		.param = NULL,
		.userid = NULL,
		.stdin_param = false,
		.verbose = 0,
		.timeout = default_timeout,
	};

	int c;
	while ((c = getopt(argc, argv, "u:t:s:ivqdVh")) != -1) {
		switch (c) {
			case 0:
				break;
			case 'u':
				conf->url = optarg;
				break;
			case 't': {
				char *end;
				conf->timeout = strtol(optarg, &end, 10);
				if (*end != '\0') {
					fprintf(stderr, "Invalid timeout: %s\n", optarg);
					exit(-1);
				}
				break;
			}
			case 's':
				conf->userid = optarg;
				break;
			case 'i':
				conf->stdin_param = true;
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
				exit(-1);
		}
	}

	if (optind < argc) {
		conf->path = argv[optind];
		char *colon = strchr(argv[optind], ':');
		if (colon) {
			*colon = '\0';
			conf->method = colon + 1;
		} else
			conf->method = "ls";
		optind++;
	}
	if (optind < argc && !conf->stdin_param)
		conf->param = argv[optind++];
	if (optind < argc) {
		fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
		exit(-1);
	}
}
