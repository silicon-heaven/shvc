#ifndef TMPDIR_H
#define TMPDIR_H


extern char *tmpdir;

void setup_tmpdir(void);

void teardown_tmpdir(void);

char *tmpdir_path(const char *name);

#endif
