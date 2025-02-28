#ifndef FOO_H
#define FOO_H
#include <stdio.h>

/** Count number of foo lines in data read from provided file.
 *
 * :param f: file object open for reading
 * :return: number of "foo:" prefix lines found in file.
 */
unsigned count_foo(FILE *f) __attribute__((nonnull));

#endif
