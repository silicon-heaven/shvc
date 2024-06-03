#include <shv/rpcdir.h>
#include <stdlib.h>

#include "rpcdir_key.gperf.h"

struct rpcdir rpcdir_ls = {
	.name = "ls",
	.param = "ils",
	.result = "ols",
	.flags = 0,
	.access = RPCMSG_ACC_BROWSE,
	.signals = (struct rpcdirsig[]){{.name = "lsmod", .param = "lsmod"}, {}},
};

struct rpcdir rpcdir_dir = {
	.name = "dir",
	.param = "idir",
	.result = "odir",
	.flags = 0,
	.access = RPCMSG_ACC_BROWSE,
};

bool rpcdir_pack(cp_pack_t pack, const struct rpcdir *method) {
	cp_pack_imap_begin(pack);

	cp_pack_int(pack, RPCDIR_KEY_NAME);
	cp_pack_str(pack, method->name);

	cp_pack_int(pack, RPCDIR_KEY_FLAGS);
	cp_pack_int(pack, method->flags);

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

	if (method->signals) {
		cp_pack_int(pack, RPCDIR_KEY_SIGNALS);
		cp_pack_map_begin(pack);
		for (size_t i = 0; method->signals[i].name; i++) {
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
			break;
		case RPCDIR_KEY_FLAGS:
			cp_unpack_int(unpack, item, res->flags);
			break;
		case RPCDIR_KEY_PARAM:
			res->param = cp_unpack_strdupo(unpack, item, obstack);
			break;
		case RPCDIR_KEY_RESULT:
			res->result = cp_unpack_strdupo(unpack, item, obstack);
			break;
		case RPCDIR_KEY_ACCESS:
			// TODO support string for backward compatibility
			cp_unpack_int(unpack, item, res->access);
			break;
		case RPCDIR_KEY_SIGNALS:
			if (cp_unpack_type(unpack, item) != CPITEM_MAP)
				break;
			struct rpcdirsig *sigs = NULL;
			size_t sigcnt = 0;
			while (cp_unpack_type(unpack, item) == CPITEM_STRING) {
				struct rpcdirsig *nsigs = realloc(sigs, ++sigcnt * sizeof *nsigs);
				if (nsigs == NULL) {
					free(sigs);
					obstack_free(obstack, res);
					return false;
				}
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
			if (sigs)
				res->signals = obstack_copy(obstack, sigs, sigcnt * sizeof *sigs);
			free(sigs);
			break;
		default: /* skip values for unknown or unsupported keys */
			cp_unpack_skip(unpack, item);
			break;
	}
	return true;
}

struct rpcdir *rpcdir_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	struct rpcdir *res = obstack_alloc(obstack, sizeof *res);
	*res = (struct rpcdir){
		.name = NULL, .param = NULL, .result = NULL, .flags = 0, .signals = NULL};
	bool has_access = false;
	switch (cp_unpack_type(unpack, item)) {
		case CPITEM_IMAP:
			for_cp_unpack_imap(unpack, item, ikey) {
				if (!unpack_value(unpack, item, obstack, res, ikey))
					return NULL;
			}
			break;
		case CPITEM_MAP: /* Compatibility with older implementations */
			for_cp_unpack_map(unpack, item, key, 11) {
				const struct gperf_rpcdir_key_match *match =
					gperf_rpcdir_key(key, strlen(key));
				if (match && !unpack_value(unpack, item, obstack, res, match->key))
					return NULL;
			}
			break;
		default:
			break;
	}
	if (res->name == NULL || !has_access) {
		obstack_free(obstack, res);
		return NULL;
	}
	return res;
}
