#include "mount.h"

static int mntcmp(const void *a, const void *b) {
	const struct mount *da = a;
	const struct mount *db = b;
	return strcmp(da->path, db->path);
}

bool mount_register(struct clientctx *c) {
	for (size_t i = 0; i < c->broker->mounts_cnt; i++)
		if (is_path_prefix(c->broker->mounts[i].path, c->role->mount_point))
			return false;
	if (mounted_client(c->broker, c->role->mount_point, NULL))
		return false;
	struct mount *mnt = ARR_ADD(c->broker->mounts);
	*mnt = (struct mount){
		.path = c->role->mount_point,
		.cid = c - c->broker->clients,
	};
	ARR_QSORT(c->broker->mounts, mntcmp);
	return true;
}

void mount_unregister(struct clientctx *c) {
	struct mount ref = {.path = c->role->mount_point};
	struct mount *mnt = ARR_BSEARCH(&ref, c->broker->mounts, mntcmp);
	if (!mnt)
		return;
	ARR_DEL(c->broker->mounts, mnt);
}

struct clientctx *mounted_client(
	struct rpcbroker *broker, const char *path, const char **rpath) {
	/* All clients access mount point. */
	const char *const clientmnt = ".broker/client/";
	if (!strncmp(path, clientmnt, strlen(clientmnt))) {
		char *end;
		long cid = strtol(path + strlen(clientmnt), &end, 10);
		if ((*end != '/' && *end != '\0') || !cid_active(broker, cid))
			return NULL;
		if (rpath)
			*rpath = end + (*end == '\0' ? 0 : 1);
		return &broker->clients[cid];
	}

	/* Now the real mount points */
	if (broker->mounts_cnt == 0)
		return NULL;
	ssize_t l = 0, u = broker->mounts_cnt - 1;
	while (l <= u) {
		size_t p = (u + l) / 2;
		const char *mnt = broker->mounts[p].path;
		int r = strncmp(path, mnt, strlen(mnt));
		if (r == 0 && is_path_prefix(path, mnt)) {
			size_t mntlen = strlen(mnt);
			if (rpath)
				*rpath = path + mntlen + (path[mntlen] == '/' ? 1 : 0);
			return &broker->clients[broker->mounts[p].cid];
		} else if (r < 0)
			u = p - 1;
		else
			l = p + 1;
	}
	return NULL;
}
