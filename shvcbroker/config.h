#ifndef _SHVCBROKER_CONFIG_H_
#define _SHVCBROKER_CONFIG_H_

#include <stdlib.h>
#include <obstack.h>
#include <shv/rpcaccess.h>
#include <shv/rpcri.h>
#include <shv/rpcurl.h>

struct user {
	const char *name;
	char *password;
	enum rpclogin_type login_type;
	char *role;
};

struct role {
	const char *name;
	char **ri_access[RPCACCESS_ADMIN + 1];
	char **ri_mount_points;
};

struct autosetup {
	char **device_ids;
	char **roles;
	char *mount_point;
	char **subscriptions;
};

struct config {
	char *name;

	struct rpcurl **listen;
	size_t listen_cnt;

	struct role *roles;
	size_t roles_cnt;

	struct user *users;
	size_t users_cnt;

	struct autosetup *autosetups;
	size_t autosetups_cnt;
};

/* Load configuration file. */
struct config *config_load(const char *path, struct obstack *obstack)
	__attribute__((nonnull));

/*! Functions used for sorting and then binary search */
int cmp_user(const void *a, const void *b) __attribute__((nonnull));
int cmp_role(const void *a, const void *b) __attribute__((nonnull));

__attribute__((nonnull(1))) static inline struct user *config_get_user(
	struct config *conf, const char *name) {
	struct user ref = {.name = name ?: ""};
	return bsearch(&ref, conf->users, conf->users_cnt, sizeof ref, cmp_user);
}

__attribute__((nonnull(1))) static inline struct role *config_get_role(
	struct config *conf, const char *name) {
	struct role ref = {.name = name};
	return bsearch(&ref, conf->roles, conf->roles_cnt, sizeof ref, cmp_role);
}

struct autosetup *config_get_autosetup(struct config *conf,
	const char *device_id, char *role) __attribute__((nonnull(1, 3)));

#endif
