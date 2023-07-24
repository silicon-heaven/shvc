#ifndef _PACKUNPACK_H_
#define _PACKUNPACK_H_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <check.h>
#include <shv/cpcp.h>

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

extern struct cpcp_pack_context pack;
extern struct cpcp_unpack_context unpack;

extern uint8_t *stashbuf;
extern size_t stashsiz;

#define ck_assert_stash(v, len) \
	do { \
		ck_assert_uint_eq(pack.current - pack.start, len); \
		ck_assert_mem_eq(stashbuf, v, len); \
	} while (false)

#define ck_assert_stash(v, len) \
	do { \
		ck_assert_uint_eq(pack.current - pack.start, len); \
		ck_assert_mem_eq(stashbuf, v, len); \
	} while (false)

#define ck_assert_stashstr(v) \
	do { \
		cpcp_pack_copy_byte(&pack, '\0'); \
		ck_assert_str_eq((char *)stashbuf, v); \
	} while (false)

void setup_pack(void);
void teardown_pack(void);

#endif
