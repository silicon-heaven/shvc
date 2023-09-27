#include <shv/rpcmsg.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <obstack.h>


static bool unpack_string(cp_unpack_t unpack, struct cpitem *item, char **dest,
	ssize_t limit, struct obstack *obstack) {
	if (obstack) {
		*dest = cp_unpack_strndupo(
			unpack, item, limit >= 0 ? limit : SIZE_MAX, obstack);
		return *dest != NULL;
	}
	cp_unpack_strncpy(unpack, item, *dest, limit);
	return true;
}

struct obscookie {
	struct obstack *obstack;
	ssize_t limit;
};

static ssize_t obstack_write(void *cookie, const char *buf, size_t size) {
	struct obscookie *c = cookie;
	// TODO limit
	obstack_grow(c->obstack, buf, size);
	return size;
}

static bool repack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_ptr *ptr, ssize_t limit, struct obstack *obstack) {
	struct obscookie cookie;
	FILE *f;
	if (obstack) {
		cookie = (struct obscookie){.obstack = obstack, .limit = limit};
		f = fopencookie(
			&cookie, "w", (cookie_io_functions_t){.write = obstack_write});
		setvbuf(f, NULL, _IONBF, 0);
	} else
		f = fmemopen(ptr->ptr, limit, "w");

	uint8_t buf[BUFSIZ];
	item->buf = buf;
	item->bufsiz = BUFSIZ;
	bool res = true;
	unsigned depth = 0;
	do {
		if (cp_unpack_type(unpack, item) == CPITEM_INVALID ||
			chainpack_pack(f, item) == 0) {
			/* Note: chainpack_pack Can fail only due to not enough space in the
			 * buffer.
			 */
			res = false;
			break;
		}
		switch (item->type) {
			case CPITEM_LIST:
			case CPITEM_MAP:
			case CPITEM_IMAP:
			case CPITEM_META:
				depth++;
				break;
			case CPITEM_CONTAINER_END:
				depth--;
				break;
			default:
				break;
		}
	} while (depth > 0);
	item->bufsiz = 0;
	fclose(f);
	if (obstack) {
		ptr->siz = obstack_object_size(obstack);
		ptr->ptr = obstack_finish(obstack);
	}
	return res;
}

static bool unpack_cids(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_ptr *ptr, ssize_t limit, struct obstack *obstack) {
	return repack(unpack, item, ptr, limit, obstack);
}

static bool unpack_extra(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta_extra **extra, struct obstack *obstack) {
	while (*extra != NULL)
		extra = &(*extra)->next;
	*extra = obstack_alloc(obstack, sizeof **extra);
	(*extra)->key = item->as.Int;
	(*extra)->next = NULL;
	return repack(unpack, item, &(*extra)->ptr, -1, obstack);
}

bool rpcmsg_head_unpack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta *meta, struct rpcmsg_meta_limits *limits,
	struct obstack *obstack) {
	meta->type = RPCMSG_T_INVALID;
	if (!cp_unpack(unpack, item))
		return false;

	meta->request_id = INT64_MIN;
	meta->access_grant = RPCMSG_ACC_INVALID;
	meta->cids.siz = 0;
	if (obstack) {
		meta->path = NULL;
		meta->method = NULL;
	} else {
		meta->path[0] = '\0';
		meta->method[0] = '\0';
	}

	/* Go through meta and parse what we want from it */
	bool has_rid = false;
	bool valid_path = true;
	bool valid_method = false;
	bool valid_cids = true;
	while (true) {
		cp_unpack(unpack, item);
		if (item->type == CPITEM_CONTAINER_END)
			break;
		if (item->type != CPITEM_INT)
			return false;
		switch (item->as.Int) {
			case RPCMSG_TAG_META_TYPE_ID:
			case RPCMSG_TAG_META_TYPE_NAMESPACE_ID:
				break;
			case RPCMSG_TAG_REQUEST_ID:
				has_rid = true;
				if (!cp_unpack_int(unpack, item, meta->request_id))
					return false;
				break;
			case RPCMSG_TAG_SHV_PATH:
				valid_path = unpack_string(unpack, item, &meta->path,
					limits ? limits->path : -1, obstack);
				break;
			case RPCMSG_TAG_METHOD:
				valid_method = unpack_string(unpack, item, &meta->method,
					limits ? limits->method : -1, obstack);
				break;
			case RPCMSG_TAG_CALLER_IDS:
				valid_cids = unpack_cids(unpack, item, &meta->cids,
					limits ? limits->cids : -1, obstack);
				break;
			case RPCMSG_TAG_ACCESS_GRANT:
				meta->access_grant = rpcmsg_access_unpack(unpack, item);
				break;
			default:
				if (limits && limits->extra)
					unpack_extra(unpack, item, &meta->extra, obstack);
				else
					cp_unpack_skip(unpack, item);
				break;
		}
	}
	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return false;
	int key = -1;
	if (cp_unpack_type(unpack, item) != CPITEM_CONTAINER_END &&
		!cpitem_extract_int(item, key))
		return false;
	if (has_rid) {
		if (valid_method) {
			meta->type = RPCMSG_T_REQUEST;
			return valid_path && valid_cids &&
				(key == RPCMSG_KEY_PARAMS || key == -1);
		}
		if (key == RPCMSG_KEY_RESULT || key == -1)
			meta->type = RPCMSG_T_RESPONSE;
		else if (key == RPCMSG_KEY_ERROR)
			meta->type = RPCMSG_T_ERROR;
		else
			return false;
		return true;
	} else {
		if (valid_method) {
			meta->type = RPCMSG_T_SIGNAL;
			return valid_path && (key == RPCMSG_KEY_PARAMS || key == -1);
		}
		return false;
	}
}
