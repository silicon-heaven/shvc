#include <stdlib.h>
#include <shv/rpcdir.h>

#include "rpcdir_key.gperf.h"

struct rpcdir rpcdir_ls = {
	.name = "ls",
	.param = "s|n",
	.result = "[s]|b",
	.flags = 0,
	.access = RPCACCESS_BROWSE,
	.signals = (struct rpcdirsig[]){{.name = "lsmod", .param = "{b}"}},
	.signals_cnt = 1,
};

struct rpcdir rpcdir_dir = {
	.name = "dir",
	.param = "n|b|s",
	.result = "[!dir]|b",
	.flags = 0,
	.access = RPCACCESS_BROWSE,
	.signals_cnt = 0,
};

bool rpcdir_pack(cp_pack_t pack, const struct rpcdir *method) {
	cp_pack_imap_begin(pack);

	cp_pack_int(pack, RPCDIR_KEY_NAME);
	cp_pack_str(pack, method->name);

	if (method->flags) {
		cp_pack_int(pack, RPCDIR_KEY_FLAGS);
		cp_pack_int(pack, method->flags);
	}

	if (method->param) {
		cp_pack_int(pack, RPCDIR_KEY_PARAM);
		cp_pack_str(pack, method->param);
	}

	if (method->result) {
		cp_pack_int(pack, RPCDIR_KEY_RESULT);
		cp_pack_str(pack, method->result);
	}

	cp_pack_int(pack, RPCDIR_KEY_ACCESS);
	cp_pack_int(pack, method->access);

	if (method->signals_cnt) {
		cp_pack_int(pack, RPCDIR_KEY_SIGNALS);
		cp_pack_map_begin(pack);
		for (size_t i = 0; i < method->signals_cnt; i++) {
			cp_pack_str(pack, method->signals[i].name);
			if (method->signals[i].param != NULL)
				cp_pack_str(pack, method->signals[i].param);
			else
				cp_pack_null(pack);
		}
		cp_pack_container_end(pack);
	}

	cp_pack_container_end(pack);
	return true;
}

static bool unpack_value(cp_unpack_t unpack, struct cpitem *item,
	struct obstack *obstack, struct rpcdir *res, enum rpcdir_keys key) {
	switch (key) {
		case RPCDIR_KEY_NAME:
			res->name = cp_unpack_strdupo(unpack, item, obstack);
			return res->name != NULL;
		case RPCDIR_KEY_FLAGS:
			return cp_unpack_int(unpack, item, res->flags);
		case RPCDIR_KEY_PARAM:
			res->param = cp_unpack_strdupo(unpack, item, obstack);
			/* Intentionally on failure keep NULL */
			return true;
		case RPCDIR_KEY_RESULT:
			res->result = cp_unpack_strdupo(unpack, item, obstack);
			/* Intentionally on failure keep NULL */
			return true;
		case RPCDIR_KEY_ACCESS:
			switch (cp_unpack_type(unpack, item)) {
				case CPITEM_INT:
					return cpitem_extract_int(item, res->access);
				case CPITEM_STRING: {
					char buf[6]; /* The longest access has 4 characters */
					bool isstr = cp_unpack_strncpy(unpack, item, buf, 6) < 6;
					if (isstr)
						res->access = rpcaccess_granted_extract(buf);
					cp_unpack_drop(unpack, item);
					return isstr;
				}
				default:
					return false;
			}
		case RPCDIR_KEY_SIGNALS:
			if (cp_unpack_type(unpack, item) != CPITEM_MAP)
				return false;
			struct rpcdirsig *sigs = NULL;
			size_t sigcnt = 0;
			while (cp_unpack_type(unpack, item) == CPITEM_STRING) {
				struct rpcdirsig *nsigs = realloc(sigs, ++sigcnt * sizeof *nsigs);
				if (nsigs == NULL) { // GCOVR_EXCL_START malloc failure only
					free(sigs);
					obstack_free(obstack, res);
					return false;
				} // GCOVR_EXCL_STOP
				sigs = nsigs;
				sigs[sigcnt - 1].name = cp_unpack_strdupo(unpack, item, obstack);
				if (cp_unpack_type(unpack, item) == CPITEM_STRING) {
					sigs[sigcnt - 1].param =
						cp_unpack_strdupo(unpack, item, obstack);
				} else {
					cp_unpack_drop(unpack, item);
					sigs[sigcnt - 1].param = NULL;
				}
			}
			if (sigs) {
				res->signals = obstack_copy(obstack, sigs, sigcnt * sizeof *sigs);
				res->signals_cnt = sigcnt;
			}
			free(sigs);
			return true;
		default: /* skip values for unknown or unsupported keys */
			cp_unpack_skip(unpack, item);
			return true;
	}
}

struct rpcdir *rpcdir_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	struct rpcdir *res = obstack_alloc(obstack, sizeof *res);
	*res = (struct rpcdir){
		.name = NULL,
		.param = NULL,
		.result = NULL,
		.flags = 0,
		.access = RPCACCESS_NONE,
		.signals = NULL,
		.signals_cnt = 0,
	};
	switch (cp_unpack_type(unpack, item)) {
		case CPITEM_IMAP:
			for_cp_unpack_imap(unpack, item, ikey) {
				if (!unpack_value(unpack, item, obstack, res, ikey))
					goto err;
			}
			break;
		case CPITEM_MAP: /* Compatibility with older implementations */
			for_cp_unpack_map(unpack, item, key, 11) {
				const struct gperf_rpcdir_key_match *match =
					gperf_rpcdir_key(key, strlen(key));
				if (!match)
					cp_unpack_skip(unpack, item);
				else if (!unpack_value(unpack, item, obstack, res, match->key))
					goto err;
			}
			break;
		default:
			break;
	}
	if (res->name == NULL || res->access == RPCACCESS_NONE)
		goto err;
	return res;

err:
	obstack_free(obstack, res);
	return NULL;
}
