#ifndef PACKSTREAM_H
#define PACKSTREAM_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <shv/cp_pack.h>


extern char *packbuf;
extern size_t packbufsiz;
extern FILE *packstream;

#define ck_assert_packbuf(v, len) \
	do { \
		fflush(packstream); \
		ck_assert_uint_eq(packbufsiz, len); \
		ck_assert_mem_eq(packbuf, v, len); \
	} while (false)

#define ck_assert_packstr(v) \
	do { \
		fputc('\0', packstream); \
		fflush(packstream); \
		ck_assert_str_eq((char *)packbuf, v); \
	} while (false)

void setup_packstream(void);
void teardown_packstream(void);


extern cp_pack_t packstream_pack;

void setup_packstream_pack_chainpack(void);
void setup_packstream_pack_cpon(void);
void teardown_packstream_pack(void);


#endif
