#include "shv/cp.h"
#include <shv/rpcmsg.h>
#include <stdio.h>
#include <assert.h>
#include <obstack.h>


static bool unpack_string(cp_unpack_t unpack, struct cpitem *item, char **dest,
	size_t limit, struct obstack *obstack) {
	if (obstack) {
		do {
			/* We are using here fast growing in a reverse way. We first store
			 * data and only after that we grow object. We do this because we
			 * do not know number of needed bytes upfront. This way we do not
			 * have to use temporally buffer and copy data around. We copy it
			 * directly to the obstack's buffer.
			 */
			// TODO ensure that limit is considered
			char *ptr = obstack_next_free(obstack);
			int siz = obstack_room(obstack);
			if (siz == 0) {
				obstack_1grow(obstack, '\0');
				siz = obstack_room(obstack) + 1;
				assert(siz > 0);
			}
			ssize_t res = cp_unpack_strncpy(unpack, item, ptr, siz);
			if (res == -1) {
				obstack_free(obstack, obstack_base(obstack));
				return false;
			}
			obstack_blank_fast(obstack, res);
			if (res < siz)
				break;
		} while (true);
		obstack_1grow(obstack, '\0');
		*dest = obstack_finish(obstack);
	} else
		cp_unpack_strncpy(unpack, item, *dest, limit);
	return true;
}

ssize_t obstack_write(void *cookie, const char *buf, size_t size) {
	struct obstack *obstack = cookie;
	obstack_grow(obstack, buf, size);
	return size;
}

static bool unpack_cids(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_ptr *ptr, size_t limit, struct obstack *obstack) {
	FILE *f;
	if (obstack) {
		// TODO respect limit
		f = fopencookie(
			obstack, "w", (cookie_io_functions_t){.write = obstack_write});
		setvbuf(f, NULL, _IONBF, 0);
	} else
		f = fmemopen(ptr->ptr, limit, "w");

	bool res = true;
	unsigned depth = 0;
	do {
		cp_unpack(unpack, item);
		if (item->type == CP_ITEM_INVALID || chainpack_pack(f, item) == -1) {
			/* Note: chainpack_pack Can fail only due to not enough space in the
			 * buffer.
			 * In case of cids we consider it as a meta parsing failure because
			 * without it we can't send response correctly anyway.
			 */
			res = false;
			break;
		}
		switch (item->type) {
			case CP_ITEM_LIST:
			case CP_ITEM_MAP:
			case CP_ITEM_IMAP:
			case CP_ITEM_META:
				depth++;
				break;
			case CP_ITEM_CONTAINER_END:
				depth--;
				break;
			default:
				break;
		}
	} while (depth > 0);
	fclose(f);
	if (obstack) {
		ptr->siz = obstack_object_size(obstack);
		ptr->ptr = obstack_finish(obstack);
	}
	return res;
}

bool rpcmsg_meta_unpack(cp_unpack_t unpack, struct cpitem *item,
	struct rpcmsg_meta *meta, struct rpcmsg_meta_limits *limits,
	struct obstack *obstack) {
	meta->type = RPCMSG_T_INVALID;
	cp_unpack(unpack, item);
	if (item->type != CP_ITEM_META)
		return false;

	bool has_rid = false;
	meta->request_id = INT64_MIN;
	meta->access_grant = RPCMSG_ACC_INVALID;

	/* Go trough meta and parse what we want from it */
	while (true) {
		cp_unpack(unpack, item);
		if (item->type == CP_ITEM_CONTAINER_END)
			break;
		if (item->type != CP_ITEM_INT)
			return false;
		switch (item->as.Int) {
			case RPCMSG_TAG_REQUEST_ID:
				cp_unpack(unpack, item);
				has_rid = true;
				if (item->type == CP_ITEM_INT)
					meta->request_id = item->as.Int;
				else if (item->type == CP_ITEM_UINT)
					meta->request_id = item->as.UInt;
				else
					return false;
				break;
			case RPCMSG_TAG_SHV_PATH:
				if (!unpack_string(unpack, item, &meta->path,
						limits ? limits->path : -1, obstack))
					return false;
				break;
			case RPCMSG_TAG_METHOD:
				if (!unpack_string(unpack, item, &meta->method,
						limits ? limits->method : -1, obstack))
					return false;
				break;
			case RPCMSG_TAG_CALLER_IDS:
				if (!unpack_cids(unpack, item, &meta->cids,
						limits ? limits->cids : -1, obstack))
					return false;
				break;
			case RPCMSG_TAG_ACCESS_GRANT:
				meta->access_grant = rpcmsg_access_unpack(unpack, item);
				break;
			default:
				cp_unpack_skip(unpack, item);
				break;
		}
	}
	meta->type = meta->method ? (has_rid ? RPCMSG_T_REQUEST : RPCMSG_T_SIGNAL)
							  : (has_rid ? RPCMSG_T_RESPONSE : RPCMSG_T_INVALID);
	return true;
}
