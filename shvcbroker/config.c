#include "config.h"
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <shv/cp_unpack.h>

#define USERNAME_MAX (255)

static void cleanup_free(void *ptr) {
	free(*(void **)ptr);
}
#define CLEANUP_FREE __attribute__((cleanup(cleanup_free)))

static void cleanup_fclose(void *ptr) {
	FILE **f = ptr;
	if (*f != NULL)
		fclose(*f);
}
#define CLEANUP_FCLOSE __attribute__((cleanup(cleanup_fclose)))

static void cleanup_cpon_free(void *ptr) {
	struct cp_unpack_cpon *unpack_cpon = ptr;
	free(unpack_cpon->state.ctx);
}
#define CLEANUP_CPON_FREE __attribute__((cleanup(cleanup_cpon_free)))

static void _unpack_args(va_list arg) {
	enum {
		NEXT_STR,
		NEXT_INT,
		NEXT_ARGS,
	} next = NEXT_STR;
	while (true) {
		switch (next) {
			case NEXT_STR:
				char *key = va_arg(arg, char *);
				if (key == NULL)
					return;
				if (!strcmp(key, "[]"))
					next = NEXT_INT;
				else if (!strcmp(key, "{}"))
					next = NEXT_ARGS;
				else
					fprintf(stderr, ".%s", key);
				break;
			case NEXT_INT:
				int ikey = va_arg(arg, int);
				fprintf(stderr, "[%d]", ikey);
				next = NEXT_STR;
				break;
			case NEXT_ARGS: {
				va_list *sarg = va_arg(arg, va_list *);
				_unpack_args(*sarg);
				next = NEXT_STR;
				break;
			}
		}
	}
}
static void unpack_error(struct cpitem *item, const char *cause, ...) {
	fputs("Config", stderr);
	va_list arg;
	va_start(arg, cause);
	_unpack_args(arg);
	va_end(arg);

	if (item->type == CPITEM_INVALID && item->as.Error != CPERR_NONE) {
		if (item->as.Error == CPERR_IO)
			fprintf(stderr, ": %s: %s\n", cperror_str(item->as.Error),
				strerror(errno));
		else
			fprintf(stderr, ": %s\n", cperror_str(item->as.Error));
	} else
		fprintf(stderr, ": %s\n", cause);
}

int cmp_user(const void *a, const void *b) {
	const struct user *da = a;
	const struct user *db = b;
	return strcmp(da->name, db->name);
}

int cmp_role(const void *a, const void *b) {
	const struct role *da = a;
	const struct role *db = b;
	return strcmp(da->name, db->name);
}

#define UNPACK_ERROR(...) \
	do { \
		unpack_error(&item, __VA_ARGS__, NULL); \
		return NULL; \
	} while (false)

#define UNPACK_VERROR(LAST, CAUSE, ...) \
	do { \
		va_list args; \
		va_start(args, LAST); \
		unpack_error(item, CAUSE, "{}", &args, __VA_ARGS__ __VA_OPT__(, ) NULL); \
		return NULL; \
	} while (false)

static char **unpack_str_list(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack, ...) {
	enum cpitem_type tp = cp_unpack_type(unpack, item);
	if (tp == CPITEM_STRING) {
		char **res = obstack_alloc(obstack, 2 * sizeof *res);
		res[0] = cp_unpack_strdupo(unpack, item, obstack);
		res[1] = NULL;
		return res;
	}
	if (tp == CPITEM_LIST) {
		size_t cnt = 0;
		CLEANUP_FREE char **list = NULL;
		for_cp_unpack_ilist(unpack, item, i) {
			if (item->type != CPITEM_STRING)
				UNPACK_VERROR(obstack, "Must be String", "[]", i);
			list = realloc(list, (cnt + 2) * sizeof *list);
			list[cnt++] = cp_unpack_strdupo(unpack, item, obstack);
		}
		list[cnt++] = NULL;
		return obstack_copy(obstack, list, cnt * sizeof *list);
	}
	UNPACK_VERROR(obstack, "Must be String or List of strings");
}

struct config *config_load(const char *path, struct obstack *obstack) {
	CLEANUP_FCLOSE FILE *f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, "Unable to open configuration file %s: %m\n", path);
		return NULL;
	}
	CLEANUP_CPON_FREE struct cp_unpack_cpon unpack_cpon;
	cp_unpack_t unpack = cp_unpack_cpon_init(&unpack_cpon, f);
	struct cpitem item;
	cpitem_unpack_init(&item);

	struct config *conf = obstack_alloc(obstack, sizeof *conf);
	*conf = (struct config){};

	if (cp_unpack_type(unpack, &item) != CPITEM_MAP)
		UNPACK_ERROR("Must be Map");
	for_cp_unpack_map(unpack, &item, key, 10) {
		if (!strcmp(key, "name")) {
			conf->name = cp_unpack_strdupo(unpack, &item, obstack);
			if (conf->name == NULL)
				UNPACK_ERROR("Must be String", key);

		} else if (!strcmp(key, "listen")) {
			if (cp_unpack_type(unpack, &item) != CPITEM_LIST)
				UNPACK_ERROR("Must be List", key);
			size_t arrsiz = 4;
			CLEANUP_FREE struct rpcurl **arr = malloc(arrsiz * sizeof *arr);
			for_cp_unpack_ilist(unpack, &item, i) {
				if (item.type != CPITEM_STRING)
					UNPACK_ERROR("Must be String", key, "[]", i);
				if (arrsiz >= i)
					arr = realloc(arr, (arrsiz *= 2) * sizeof *arr);
				char *url = cp_unpack_strdup(unpack, &item);
				arr[i] = rpcurl_parse(url, NULL, obstack);
				free(url);
				if (arr[i] == NULL)
					UNPACK_ERROR("Invalid RPC URL format", key, "[]", i);
				conf->listen_cnt = i + 1;
			}
			conf->listen =
				obstack_copy(obstack, arr, conf->listen_cnt * sizeof *arr);

		} else if (!strcmp(key, "users")) {
			if (cp_unpack_type(unpack, &item) != CPITEM_MAP)
				UNPACK_ERROR("Must be Map", key);
			size_t arrpos = 0;
			CLEANUP_FREE struct user *users = NULL;
			while (cp_unpack_type(unpack, &item) == CPITEM_STRING) {
				users = realloc(users, (arrpos + 1) * sizeof *users);
				struct user *user = &users[arrpos++];
				memset(user, 0, sizeof *user);
				user->name = cp_unpack_strdupo(unpack, &item, obstack);
				if (user->name == NULL)
					UNPACK_ERROR("Invalid Map", key);
				if (cp_unpack_type(unpack, &item) != CPITEM_MAP)
					UNPACK_ERROR("Must be Map", key, user->name);
				for_cp_unpack_map(unpack, &item, ukey, 9) {
					if (!strcmp(ukey, "password")) {
						if (cp_unpack_type(unpack, &item) != CPITEM_STRING)
							UNPACK_ERROR("Must be String", key, user->name, ukey);
						user->password = cp_unpack_strdupo(unpack, &item, obstack);
						user->login_type = RPC_LOGIN_PLAIN;
					} else if (!strcmp(ukey, "sha1pass")) {
						if (cp_unpack_type(unpack, &item) != CPITEM_STRING)
							UNPACK_ERROR("Must be String", key, user->name, ukey);
						user->password = cp_unpack_strdupo(unpack, &item, obstack);
						user->login_type = RPC_LOGIN_SHA1;
					} else if (!strcmp(ukey, "role")) {
						if (cp_unpack_type(unpack, &item) != CPITEM_STRING)
							UNPACK_ERROR("Must be String", key, user->name, ukey);
						user->role = cp_unpack_strdupo(unpack, &item, obstack);
					} else
						UNPACK_ERROR("Not expected", key, user->name, ukey);
				}
			}
			conf->users = obstack_copy(obstack, users, arrpos * sizeof *users);
			conf->users_cnt = arrpos;
			qsort(conf->users, conf->users_cnt, sizeof *conf->users, cmp_user);

		} else if (!strcmp(key, "roles")) {
			if (cp_unpack_type(unpack, &item) != CPITEM_MAP)
				UNPACK_ERROR("Must be Map", key);
			size_t arrpos = 0;
			CLEANUP_FREE struct role *roles = NULL;
			while (cp_unpack_type(unpack, &item) == CPITEM_STRING) {
				roles = realloc(roles, (arrpos + 1) * sizeof *roles);
				struct role *role = &roles[arrpos++];
				memset(role, 0, sizeof *role);
				role->name = cp_unpack_strdupo(unpack, &item, obstack);
				if (role->name == NULL)
					UNPACK_ERROR("Invalid Map", key);
				if (cp_unpack_type(unpack, &item) != CPITEM_MAP)
					UNPACK_ERROR("Must be Map", key, role->name);
				for_cp_unpack_map(unpack, &item, ukey, 12) {
					if (!strcmp(ukey, "access")) {
						if (cp_unpack_type(unpack, &item) != CPITEM_MAP)
							UNPACK_ERROR("Must be Map", key, role->name, ukey);
						for_cp_unpack_map(unpack, &item, akey, 5) {
							rpcaccess_t access = rpcaccess_granted_access(akey);
							if (access == RPCACCESS_NONE)
								UNPACK_ERROR("Invalid access level", key,
									role->name, ukey, akey);
							// TODO validate RIs
							if ((role->ri_access[access] = unpack_str_list(
									 unpack, &item, obstack, key, role->name,
									 ukey, akey, NULL)) == NULL)
								return NULL;
						}
					} else if (!strcmp(ukey, "mountPoints")) {
						if ((role->ri_mount_points = unpack_str_list(unpack, &item,
								 obstack, key, role->name, ukey, NULL)) == NULL)
							return NULL;
					} else
						UNPACK_ERROR("Not expected", key, role->name, ukey);
				}
			}
			conf->roles = obstack_copy(obstack, roles, arrpos * sizeof *roles);
			conf->roles_cnt = arrpos;
			qsort(conf->roles, conf->roles_cnt, sizeof *conf->roles, cmp_role);

		} else if (!strcmp(key, "autosetups")) {
			if (cp_unpack_type(unpack, &item) != CPITEM_LIST)
				UNPACK_ERROR("Must be List", key);
			size_t arrpos = 0;
			CLEANUP_FREE struct autosetup *autosetups = NULL;
			for_cp_unpack_ilist(unpack, &item, i) {
				if (item.type != CPITEM_MAP)
					UNPACK_ERROR("Must be Map", key, "[]", i);
				autosetups = realloc(autosetups, (arrpos + 1) * sizeof *autosetups);
				struct autosetup *autosetup = &autosetups[arrpos++];
				memset(autosetup, 0, sizeof *autosetup);
				for_cp_unpack_map(unpack, &item, akey, 14) {
					if (!strcmp(akey, "deviceId")) {
						if ((autosetup->device_ids = unpack_str_list(unpack, &item,
								 obstack, key, "[]", i, akey, NULL)) == NULL)
							return NULL;
					} else if (!strcmp(akey, "role")) {
						if ((autosetup->roles = unpack_str_list(unpack, &item,
								 obstack, key, "[]", i, akey, NULL)) == NULL)
							return NULL;
					} else if (!strcmp(akey, "mountPoint")) {
						if (cp_unpack_type(unpack, &item) != CPITEM_STRING)
							UNPACK_ERROR("Must be String", key, "[]", i, akey);
						autosetup->mount_point =
							cp_unpack_strdupo(unpack, &item, obstack);
					} else if (!strcmp(akey, "subscriptions")) {
						if ((autosetup->subscriptions = unpack_str_list(unpack,
								 &item, obstack, key, "[]", i, akey, NULL)) == NULL)
							return NULL;
					} else
						UNPACK_ERROR("Not expected", key, "[]", i, akey);
				}
			}
			conf->autosetups =
				obstack_copy(obstack, autosetups, arrpos * sizeof *autosetups);
			conf->autosetups_cnt = arrpos;

		} else
			UNPACK_ERROR("Not expected", key);
	}
	if (item.type != CPITEM_CONTAINER_END)
		UNPACK_ERROR("Unexpected item");

	for (size_t i = 0; i < conf->users_cnt; i++) {
		if (conf->users[i].role == NULL)
			UNPACK_ERROR(
				"Role must be specified", "users", conf->users[i].name, "role");
		if (config_get_role(conf, conf->users[i].role) == NULL)
			UNPACK_ERROR("Role must exist", "users", conf->users[i].name, "role");
	}

	return conf;
}


static bool rpcri_match_oneof(char **patterns, const char *string) {
	if (patterns)
		for (char **pattern = patterns; *pattern; pattern++)
			if (rpcstr_match(*pattern, string))
				return true;
	return false;
}

struct autosetup *config_get_autosetup(
	struct config *conf, const char *device_id, char *role) {
	for (size_t i = 0; i < conf->autosetups_cnt; i++) {
		struct autosetup *autosetup = &conf->autosetups[i];
		if (autosetup->device_ids &&
			!rpcri_match_oneof(autosetup->device_ids, device_id))
			continue;
		if (autosetup->roles && !rpcri_match_oneof(autosetup->roles, role))
			continue;
		return autosetup;
	}
	return NULL;
}
