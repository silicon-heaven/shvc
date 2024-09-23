/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#ifndef FOO_H
#define FOO_H
#include <stdio.h>

/*! Count number of foo lines in data read from provided file.
 * @param f: file object open for reading
 * @returns number of "foo:" prefix lines found in file.
 */
unsigned count_foo(FILE *f) __attribute__((nonnull));

#endif
