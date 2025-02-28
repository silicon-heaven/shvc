#ifndef TMPDIR_H
#define TMPDIR_H


extern char *tmpdir;

void setup_tmpdir(void);

void teardown_tmpdir(void);

[[gnu::nonnull, gnu::returns_nonnull]]
char *tmpdir_path(const char *name);

[[gnu::nonnull, gnu::returns_nonnull]]
char *tmpdir_file(const char *name, const char *content);

#endif
