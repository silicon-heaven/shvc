#include "tmpdir.h"
#include <stdlib.h>
#include <stdio.h>
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

void teardown_tmpdir(void) {
	if (tmpdir == NULL)
		return;
	char *cmd;
	ck_assert_int_gt(asprintf(&cmd, "rm -rf '%s'", tmpdir), 0);
	ck_assert_int_eq(system(cmd), 0);
	free(cmd);
	free(tmpdir);
	tmpdir = NULL;
}

char *tmpdir_path(const char *name) {
	char *res;
	ck_assert_int_gt(asprintf(&res, "%s/%s", tmpdir, name), 0);
	return res;
}
