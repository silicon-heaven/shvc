#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "config.h"


int main(int argc, char **argv) {
	load_config(argc, argv);

	return 0;
}
