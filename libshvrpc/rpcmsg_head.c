#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <obstack.h>
#include <shv/rpcmsg.h>


const struct rpcmsg_meta_limits rpcmsg_meta_limits_default = {
	// TODO make this configurable with meson_options
	.path = 1024,
	.name = 256,
	.user_id = 256,
	.extra = false,
};


ssize_t obswrite(void *cookie, const char *buf, size_t size) {
	struct obstack *obstack = cookie;
	obstack_grow(obstack, buf, size);
	return size;
}

static struct rpcmsg_meta_extra *unpack_extra(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	struct rpcmsg_meta_extra *res = obstack_alloc(obstack, sizeof *res);
	*res = (struct rpcmsg_meta_extra){
		.key = item->as.Int,
		.siz = 0,
		.next = NULL,
	};
	FILE *f =
		fopencookie(obstack, "w", (cookie_io_functions_t){.write = obswrite});
	setbuf(f, NULL);
	uint8_t buf[BUFSIZ];
	item->buf = buf;
	item->bufsiz = BUFSIZ;
	for_cp_unpack_item(unpack, item, 0) {
		chainpack_pack(f, item);
	}
	item->bufsiz = 0;
	fclose(f);
	res->siz = obstack_object_size(obstack);
	res->ptr = obstack_finish(obstack);
	return res;
}

static inline char *unpack_user_id_compat(cp_unpack_t unpack,
	struct cpitem *item, struct obstack *obstack, ssize_t limit) {
	char *brokerId = NULL;
	char *shvUser = NULL;
	for_cp_unpack_map(unpack, item, key, 8) {
		if (!strcmp("brokerId", key)) {
			brokerId = cp_unpack_strndup(unpack, item, limit);
			limit -= strlen(brokerId) + (shvUser ? 1 : 0);
		} else if (!strcmp("shvUser", key)) {
			shvUser = cp_unpack_strndup(unpack, item, limit);
			limit -= strlen(shvUser) + (brokerId ? 1 : 0);
		} /* Ignore any other keys */
	}
	if (brokerId) {
		obstack_grow(obstack, brokerId, strlen(brokerId));
		free(brokerId);
	}
	if (shvUser) {
		if (brokerId)
			obstack_1grow(obstack, ':');
		obstack_grow(obstack, shvUser, strlen(shvUser));
		free(shvUser);
	}
	obstack_1grow(obstack, '\0');
	return obstack_finish(obstack);
}

bool rpcmsg_head_unpack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta *meta, const struct rpcmsg_meta_limits *limits,
	struct obstack *obstack) {
	*meta = (struct rpcmsg_meta){
		.type = RPCMSG_T_INVALID,
		.access = RPCACCESS_NONE,
		.path = NULL,
		.method = NULL, /* Also covers .signal */
		.source = NULL,
		.repeat = false,
		.user_id = NULL,
		.cids_cnt = 0,
		.extra = NULL,
	};

	if (limits == NULL)
		limits = &rpcmsg_meta_limits_default;

	if (cp_unpack_type(unpack, item) != CPITEM_META)
		return false;
	void *obase = obstack_base(obstack);

#define FAILURE \
	do { \
		obstack_free(obstack, obase); \
		return false; \
	} while (false)
#define STRDUPO(limit) \
	({ \
		char *res = (limit) >= 0 \
			? cp_unpack_strndupo(unpack, item, (limit), obstack) \
			: cp_unpack_strdupo(unpack, item, obstack); \
		if (res == NULL) \
			FAILURE; \
		res; \
	})

	bool has_rid = false;
	bool has_access = false;
	struct rpcmsg_meta_extra **prevextra = &meta->extra;
	while (cp_unpack_type(unpack, item) != CPITEM_CONTAINER_END) {
		if (item->type == CPITEM_STRING) {
			cp_unpack_skip(unpack, item);
			continue; /* Ignore string keys that we do not support */
		}
		if (item->type != CPITEM_INT) // GCOVR_EXCL_BR_LINE
			FAILURE; // GCOVR_EXCL_LINE covers unlikely invalid meta
		switch (item->as.Int) {
			case RPCMSG_TAG_META_TYPE_ID:
				if (cp_unpack_type(unpack, item) != CPITEM_INT || item->as.Int != 1)
					FAILURE; // GCOVR_EXCL_BR_LINE
				break;
			case RPCMSG_TAG_META_TYPE_NAMESPACE_ID:
				if (cp_unpack_type(unpack, item) != CPITEM_INT || item->as.Int != 0)
					FAILURE; // GCOVR_EXCL_BR_LINE
				break;
			case RPCMSG_TAG_REQUEST_ID:
				if (!cp_unpack_int(unpack, item, meta->request_id))
					FAILURE; // GCOVR_EXCL_BR_LINE
				has_rid = true;
				break;
			case RPCMSG_TAG_SHV_PATH:
				meta->path = STRDUPO(limits->path);
				break;
			case RPCMSG_TAG_METHOD: /* RPCMSG_TAG_SIGNAL */
				meta->method = STRDUPO(limits->name);
				break;
			case RPCMSG_TAG_CALLER_IDS:
				cp_unpack(unpack, item);
				if (item->type == CPITEM_INT) {
					meta->cids =
						obstack_copy(obstack, &item->as.Int, sizeof(long long));
					meta->cids_cnt = 1;
					break;
				} else if (item->type != CPITEM_LIST)
					FAILURE; // GCOVR_EXCL_BR_LINE
				/* Set to zero just in case key is specified multiple times */
				meta->cids_cnt = 0;
				for_cp_unpack_list(unpack, item) {
					if (item->type != CPITEM_INT)
						FAILURE; // GCOVR_EXCL_BR_LINE
					obstack_grow(obstack, &item->as.Int, sizeof(long long));
					meta->cids_cnt++;
				}
				meta->cids = obstack_finish(obstack);
				break;
			case RPCMSG_TAG_ACCESS:
				/* Process only if we haven't seen access level or if extra is
				 * requested.
				 */
				meta->access_granted = STRDUPO(128);
				rpcaccess_t acc = rpcaccess_granted_extract(meta->access_granted);
				if (meta->access_granted[0] == '\0') {
					/* Do not keep empty string around */
					obstack_free(obstack, meta->access_granted);
					meta->access_granted = NULL;
				}
				if (!has_access) { /* We prefer the new access level */
					meta->access = acc;
					has_access = true;
				}
				break;
			case RPCMSG_TAG_USER_ID:
				switch (cp_unpack_type(unpack, item)) {
					case CPITEM_MAP:
						meta->user_id = unpack_user_id_compat(
							unpack, item, obstack, limits->user_id - 1);
						break;
					case CPITEM_STRING:
						meta->user_id = STRDUPO(limits->user_id);
						break;
					default:
						FAILURE; // GCOVR_EXCL_BR_LINE
				}
				break;
			case RPCMSG_TAG_ACCESS_LEVEL:
				if (cp_unpack_type(unpack, item) != CPITEM_INT)
					FAILURE; // GCOVR_EXCL_BR_LINE
				meta->access = item->as.Int;
				has_access = true;
				break;
			case RPCMSG_TAG_SOURCE:
				meta->source = STRDUPO(limits->name);
				break;
			case RPCMSG_TAG_REPEAT:
				if (cp_unpack_type(unpack, item) != CPITEM_BOOL)
					FAILURE; // GCOVR_EXCL_BR_LINE
				meta->repeat = item->as.Bool;
				break;
			default:
				if (!limits->extra) {
					cp_unpack_skip(unpack, item);
					break;
				}
				*prevextra = unpack_extra(unpack, item, obstack);
				prevextra = &(*prevextra)->next;
				break;
		}
	}
	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		FAILURE; // GCOVR_EXCL_BR_LINE
	int key = 0;
	if (!cp_unpack_int(unpack, item, key) && // GCOVR_EXCL_BR_LINE
		item->type != CPITEM_CONTAINER_END)
		FAILURE; // GCOVR_EXCL_LINE covers unlikely invalid imap

	if (has_rid) {
		if (meta->method) {
			/* Request */
			if (key && key != RPCMSG_KEY_PARAM)
				FAILURE; // GCOVR_EXCL_BR_LINE
			meta->type = RPCMSG_T_REQUEST;
			if (meta->path == NULL)
				meta->path = "";
			if (!has_access)
				meta->access = RPCACCESS_ADMIN;
		} else {
			/* Response */
			if (!key || key == RPCMSG_KEY_RESULT) {
				meta->type = RPCMSG_T_RESPONSE;
			} else if (key == RPCMSG_KEY_ERROR) {
				meta->type = RPCMSG_T_ERROR;
			} else
				FAILURE; // GCOVR_EXCL_BR_LINE
		}
	} else {
		/* Signal */
		if (key && key != RPCMSG_KEY_PARAM)
			FAILURE; // GCOVR_EXCL_BR_LINE
		meta->type = RPCMSG_T_SIGNAL;
		if (meta->path == NULL)
			meta->path = "";
		if (!has_access)
			meta->access = RPCACCESS_READ;
		if (!meta->signal)
			meta->signal = "chng";
		if (!meta->source)
			meta->source = "get";
	}
	return true;
#undef FAILURE
#undef STRDUPO
}
