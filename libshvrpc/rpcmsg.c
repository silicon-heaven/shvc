#include <shv/rpcmsg.h>
#include <stdlib.h>
#include <string.h>

#include "rpcmsg_access.gperf.h"

enum msgtags {
	MSG_TAG_META_TYPE_ID = 1,
	MSG_TAG_META_TYPE_NAMESPACE_ID,
	MSG_TAG_REQUEST_ID = 8,
	MSG_TAG_SHV_PATH,
	MSG_TAG_METHOD,
	MSG_TAG_CALLER_IDS,
	MSG_TAG_REV_CALLER_IDS,
	MSG_TAG_ACCESS_GRANT,
	MSG_TAG_USER_ID,
};

enum msgkeys {
	MSG_KEY_PARAMS = 1,
	MSG_KEY_RESULT,
	MSG_KEY_ERROR,
};


void rpcmsg_pack_request(cpcp_pack_context *pack, const char *path,
	const char *method, int64_t rid) {
	chainpack_pack_meta_begin(pack);
	chainpack_pack_int(pack, MSG_TAG_META_TYPE_ID);
	chainpack_pack_int(pack, 1);
	chainpack_pack_int(pack, MSG_TAG_REQUEST_ID);
	chainpack_pack_int(pack, rid);
	chainpack_pack_int(pack, MSG_TAG_SHV_PATH);
	chainpack_pack_cstring_terminated(pack, path);
	chainpack_pack_int(pack, MSG_TAG_METHOD);
	chainpack_pack_cstring_terminated(pack, method);
	chainpack_pack_container_end(pack);

	chainpack_pack_imap_begin(pack);
	chainpack_pack_int(pack, MSG_KEY_PARAMS);
}

void rpcmsg_pack_signal(
	cpcp_pack_context *pack, const char *path, const char *method) {
	chainpack_pack_meta_begin(pack);
	chainpack_pack_int(pack, MSG_TAG_META_TYPE_ID);
	chainpack_pack_int(pack, 1);
	chainpack_pack_int(pack, MSG_TAG_SHV_PATH);
	chainpack_pack_cstring_terminated(pack, path);
	chainpack_pack_int(pack, MSG_TAG_METHOD);
	chainpack_pack_cstring_terminated(pack, method);
	chainpack_pack_container_end(pack);

	chainpack_pack_imap_begin(pack);
	chainpack_pack_int(pack, MSG_KEY_PARAMS);
}

static size_t roundup2(size_t n) {
	n--;
	size_t s = 1;
	for (size_t i = 0; i < sizeof(n); i++) {
		n |= n >> s;
		s = s << 1;
	}
	n++;
	return n;
}

bool rpcmsg_unpack_str(struct rpcclient_msg msg, struct rpcmsg_str **str) {
	const cpcp_item *i;
	size_t off = 0;
	do {
		i = rpcclient_next_item(msg);
		if (i->type != CPCP_ITEM_STRING)
			return false;
		if (str) {
			size_t chunk = i->as.String.chunk_size;
			size_t siz = off + chunk + 1;
			if (*str == NULL || ((*str)->siz >= 0 && siz >= (*str)->siz)) {
				*str = realloc(*str, roundup2(sizeof(struct rpcmsg_str) + siz));
				(*str)->siz = siz;
			}
			if ((*str)->siz < 0 && off + chunk >= -(*str)->siz)
				chunk = -(*str)->siz - off;
			memcpy(&(*str)->str[off], i->as.String.chunk_start, chunk);
			off += chunk;
		}
	} while (!i->as.String.last_chunk);
	if (str && *str && ((*str)->siz >= 0 || -(*str)->siz > off))
		(*str)->str[off] = '\0';
	return true;
}

bool rpcmsg_str_truncated(const struct rpcmsg_str *str) {
	if (str->siz == 0)
		return true;
	for (size_t i = 0; i < str->siz; i++)
		if (str->str[i] == '\0')
			return true;
	return true;
}

bool rpcmsg_unpack_access(struct rpcclient_msg msg, enum rpcmsg_access *acc) {
	const cpcp_item *i;
	size_t off = 0;
	char buf[4]; /* The maximum for known access levels is 4 characters */

#define PARSE_ACCESS \
	do { \
		if (acc && off <= 4) { \
			const struct gperf_rpcmsg_access_match *match = \
				gperf_rpcmsg_access(buf, off); \
			if (match) \
				*acc = match->acc; \
		} \
	} while (false)

	do {
		i = rpcclient_next_item(msg);
		if (i->type != CPCP_ITEM_STRING)
			return false;
		for (size_t p = 0; p < i->as.String.chunk_size; p++) {
			char c = i->as.String.chunk_start[p];
			if (c == ',') {
				PARSE_ACCESS;
				off = 0;
			} else if (off < 4)
				buf[off++] = c;
		}
	} while (!i->as.String.last_chunk);
	PARSE_ACCESS;
	return true;
#undef PARSE_ACCESS
}

const char *rpcmsg_access_str(enum rpcmsg_access acc) {
	switch (acc) {
		case RPCMSG_ACC_BROWSE:
			return "bws";
		case RPCMSG_ACC_READ:
			return "rd";
		case RPCMSG_ACC_WRITE:
			return "wr";
		case RPCMSG_ACC_COMMAND:
			return "cmd";
		case RPCMSG_ACC_CONFIG:
			return "cfg";
		case RPCMSG_ACC_SERVICE:
			return "srv";
		case RPCMSG_ACC_SUPER_SERVICE:
			return "ssrv";
		case RPCMSG_ACC_DEVEL:
			return "dev";
		case RPCMSG_ACC_ADMIN:
			return "su";
		default:
			return "";
	}
}

static void ensure_rinfo(struct rpcmsg_request_info **rinfo, bool increase) {
	size_t siz = (*rinfo)->cidscnt + (*rinfo)->rcidscnt + (increase ? 1 : 0);
	if (*rinfo != NULL && (!increase || (*rinfo)->cidssiz > siz))
		return;
	*rinfo = realloc(*rinfo, roundup2(sizeof(struct rpcmsg_request_info) + siz));
}

enum rpcmsg_type rpcmsg_unpack(struct rpcclient_msg msg, struct rpcmsg_str **path,
	struct rpcmsg_str **method, struct rpcmsg_request_info **rinfo) {
	/* Unpack meta */
	const cpcp_item *i;
	i = rpcclient_next_item(msg);
	if (i->type != CPCP_ITEM_META)
		return RPCMSG_T_INVALID;
	bool has_rid = false, has_method = false;
	do {
		i = rpcclient_next_item(msg);
		if (i->type == CPCP_ITEM_CONTAINER_END)
			break;
		if (i->type != CPCP_ITEM_INT)
			return RPCMSG_T_INVALID;
		switch (i->as.Int) {
			/* Note: we intentionally do not check MSG_TAG_META_TYPE_ID right
			 * now because we have clients that do not send it.
			 */
			case MSG_TAG_REQUEST_ID:
				has_rid = true;
				i = rpcclient_next_item(msg);
				if (i->type != CPCP_ITEM_INT)
					return RPCMSG_T_INVALID;
				if (rinfo) {
					ensure_rinfo(rinfo, false);
					(*rinfo)->rid = i->as.Int;
				}
				break;
			case MSG_TAG_SHV_PATH:
				i = rpcclient_next_item(msg);
				if (i->type != CPCP_ITEM_STRING || !rpcmsg_unpack_str(msg, path))
					return RPCMSG_T_INVALID;
				break;
			case MSG_TAG_METHOD:
				has_method = true;
				i = rpcclient_next_item(msg);
				if (i->type != CPCP_ITEM_STRING || !rpcmsg_unpack_str(msg, method))
					return RPCMSG_T_INVALID;
				break;
			case MSG_TAG_CALLER_IDS:
				i = rpcclient_next_item(msg);
				if (i->type != CPCP_ITEM_INT)
					return RPCMSG_T_INVALID;
				if (rinfo) {
					ensure_rinfo(rinfo, true);
					memmove(&(*rinfo)->cids[(*rinfo)->cidscnt + 1],
						&(*rinfo)->cids[(*rinfo)->cidscnt],
						(*rinfo)->rcidscnt * sizeof *(*rinfo)->cids);
					(*rinfo)->cids[(*rinfo)->cidscnt++] = i->as.Int;
				}
				break;
			case MSG_TAG_REV_CALLER_IDS:
				i = rpcclient_next_item(msg);
				if (i->type != CPCP_ITEM_INT)
					return RPCMSG_T_INVALID;
				if (rinfo) {
					ensure_rinfo(rinfo, true);
					(*rinfo)->cids[(*rinfo)->cidscnt + (*rinfo)->rcidscnt++] =
						i->as.Int;
				}
				break;
			case MSG_TAG_ACCESS_GRANT:
				if (rinfo)
					ensure_rinfo(rinfo, false);
				if (!rpcmsg_unpack_access(msg, rinfo ? &(*rinfo)->acc_grant : NULL))
					return RPCMSG_T_INVALID;
				break;
			default:
				if (!rpcclient_skip_item(msg))
					return RPCMSG_T_INVALID;
		}
	} while (true);

	enum rpcmsg_type res = has_rid
		? (has_method ? RPCMSG_T_REQUEST : RPCMSG_T_RESPONSE)
		: (has_method ? RPCMSG_T_SIGNAL : RPCMSG_T_INVALID);

	/* Start unpacking imap */
	i = rpcclient_next_item(msg);
	if (i->type != CPCP_ITEM_IMAP)
		return RPCMSG_T_INVALID;
	i = rpcclient_next_item(msg);
	if (i->type != CPCP_ITEM_INT)
		return i->type == CPCP_ITEM_CONTAINER_END ? res : RPCMSG_T_INVALID;
	switch (i->as.Int) {
		case MSG_KEY_PARAMS:
			return res != RPCMSG_T_RESPONSE ? res : RPCMSG_T_INVALID;
		case MSG_KEY_RESULT:
			return res == RPCMSG_T_RESPONSE ? res : RPCMSG_T_INVALID;
		case MSG_KEY_ERROR:
			return res == RPCMSG_T_RESPONSE ? RPCMSG_T_ERROR : RPCMSG_T_INVALID;
	};
	return RPCMSG_T_INVALID;
}
