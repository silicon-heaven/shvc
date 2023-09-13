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

#define ck_assert_bdata(BUF, LEN, BDATA) \
	do { \
		size_t __len = LEN; \
		struct bdata __b = BDATA; \
		ck_assert_int_eq(__len, __b.len); \
		ck_assert_mem_eq(BUF, __b.v, __len); \
	} while (false)

#endif
