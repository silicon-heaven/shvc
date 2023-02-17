/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#ifndef _FOO_H_
#define _FOO_H_
#include <stdio.h>

/* Count number of foo lines in data read from provided file.
 * f: file object open for reading
 * Returns number of "foo:" prefix lines found in file.
 */
unsigned count_foo(FILE *f) __attribute__((nonnull));

#endif
