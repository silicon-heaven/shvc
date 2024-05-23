#include <stdio.h>
#include "opts.h"


int main(int argc, char **argv) {
	struct conf conf;
	parse_opts(argc, argv, &conf);

	fprintf(stderr, "This tool is not implemented yet!");
	return 1;
}
