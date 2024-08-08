#ifndef TMPDIR_H
#define TMPDIR_H


extern char *tmpdir;

void setup_tmpdir(void);

void teardown_tmpdir(void);

char *tmpdir_path(const char *name) __attribute__((nonnull));

char *tmpdir_file(const char *name, const char *content) __attribute__((nonnull));

#endif
