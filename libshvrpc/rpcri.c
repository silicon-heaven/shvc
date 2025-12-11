#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shv/rpcri.h>

static inline const char *rpcri_match_ranges(const char *p, const char *s) {
	do {
		if (*s == *p) {
			while (*p != ']' && *p != '\0')
				p++;
			return p;
		}

		char first = *p++;
		if (*p == '-') {
			p++;
			if (*s >= first && *s <= *p) {
				while (*p != ']' && *p != '\0')
					p++;
				return p;
			}
		}
	} while (*p != ']' && *p != '\0');

	return NULL;
}

static inline const char *rpcri_match_negation(const char *p, const char *s) {
	p++;
	do {
		if (*s == *p)
			return NULL;

		char first = *p++;
		if (*p == '-') {
			p++;
			if (*s >= first && *s <= *p)
				return NULL;
		}
	} while (*p != ']' && *p != '\0');

	return p;
}

// TODO we should be able to select if this should be handled as path or not
// (the slash won't be the splitter in such a case).
static bool rpcri_match_one(const char *pattern, const char *string, size_t n) {
	const char *p;
	int i;
	int pl;

	for (p = pattern; p - pattern < n; p++, string++) {
		if (*p == '?' && *string != '\0')
			continue;

		if (*p == '[') {
			if (*(p + 1) == '!') {
				if (*string == '\0') {
					while (*p != ']' && *p != '\0')
						p++;
					if (*p == '\0' || *(p + 1) == '\0')
						return true;
				}
				if (!(p = rpcri_match_negation(++p, string)))
					return false;
			} else if (*string != '\0') {
				if (!(p = rpcri_match_ranges(++p, string)))
					return false;
			}
			continue;
		}

		if (*p == '*') {
			p++;
			if (*p == '*') {
				p++;
				i = strlen(string);
			} else
				i = strcspn(string, "/");

			pl = n - (p - pattern);
			for (; i >= 0; i--) {
				if (rpcri_match_one(p, &string[i], pl))
					return true;
			}
		}
		/* foo/\** can also match foo itself. Handle this special case */
		if (*p == '/' && (n - (p - pattern)) == 3 && *(p + 1) == '*' &&
			*(p + 2) == '*')
			return true;

		if (*p != *string)
			return false;
	}

	if (*string == '\0')
		return true;

	return false;
}

bool rpcri_match(
	const char *ri, const char *path, const char *method, const char *signal) {
	char *ri_path_end = strchr(ri, ':');
	if (!ri_path_end)
		/* At least one : has to be present -> path:method. */
		return false;

	/* Match path */
	if (!rpcri_match_one(ri, path, strlen(ri) - strlen(ri_path_end)))
		return false;

	ri_path_end++;
	char *ri_method_end;
	if ((signal == NULL) || (ri_method_end = strchr(ri_path_end, ':')) == NULL)
		/* Either signal is not present in resource or in match patter. */
		return rpcri_match_one(ri_path_end, method, strlen(ri_path_end));
	else {
		/* Signal is present, first match method, then signal. */
		size_t method_len = strlen(ri_path_end) - strlen(ri_method_end);
		if (!rpcri_match_one(ri_path_end, method, method_len))
			return false;

		ri_method_end++;
		return rpcri_match_one(ri_method_end, signal, strlen(ri_method_end));
	}

	return true;
}

bool rpcpath_match(const char *pattern, const char *path) {
	return rpcri_match_one(pattern, path, strlen(pattern));
}

bool rpcstr_match(const char *pattern, const char *string) {
	return rpcri_match_one(pattern, string, strlen(pattern));
}
