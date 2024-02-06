#include "utils.h"

void print_ll_array(const long long *array, size_t length) {
	fprintf(stdout, "[");
	for (int i = 0; i < length; i++) {
		if (i + 1 == length)
			fprintf(stdout, "%lld", array[i]);
		else
			fprintf(stdout, "%lld, ", array[i]);
	}
	fprintf(stdout, "]\n");
}
