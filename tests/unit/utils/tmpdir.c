#include "tmpdir.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <check.h>


char *tmpdir = NULL;


void setup_tmpdir(void) {
	if (tmpdir)
		return;
	char *tmp = getenv("TMPDIR");
	if (tmp == NULL)
		tmp = P_tmpdir;
	ck_assert_int_gt(asprintf(&tmpdir, "%s/%s.XXXXXX", tmp, "shvc-testunit-"), 0);
	ck_assert_ptr_eq(mkdtemp(tmpdir), tmpdir);
}

static void _rmdir(int fd) {
	ck_assert_int_ge(fd, 0);
	DIR *dfd = fdopendir(fd);
	ck_assert_ptr_nonnull(dfd);
	struct dirent *de;
	while ((de = readdir(dfd)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		if (de->d_type == DT_DIR) {
			_rmdir(openat(fd, de->d_name, O_DIRECTORY));
			ck_assert_int_eq(unlinkat(fd, de->d_name, AT_REMOVEDIR), 0);
		} else
			ck_assert_int_eq(unlinkat(fd, de->d_name, 0), 0);
	}
	closedir(dfd);
}

void teardown_tmpdir(void) {
	if (tmpdir == NULL)
		return;
	_rmdir(open(tmpdir, O_DIRECTORY));
	ck_assert_int_eq(rmdir(tmpdir), 0);
	free(tmpdir);
	tmpdir = NULL;
}

char *tmpdir_path(const char *name) {
	char *res;
	ck_assert_int_gt(asprintf(&res, "%s/%s", tmpdir, name), 0);
	return res;
}
