#include "util.h"


time_t get_time(void) {
	// TODO possibly detect that we can use CLOCK_MONOTONIC_COARSE
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}
