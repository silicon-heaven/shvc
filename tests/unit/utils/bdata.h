#ifndef BDATA_H
#define BDATA_H
#include <stdint.h>
#include <sys/types.h>

/* Unitility macro that provides array of bytes with its size */
struct bdata {
	const uint8_t *const v;
	const size_t len;
};
#define B(...) \
	(struct bdata) { \
		.v = (const uint8_t[]){__VA_ARGS__}, \
		.len = sizeof((const uint8_t[]){__VA_ARGS__}), \
	}

#endif
