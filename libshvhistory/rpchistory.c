#include <stdlib.h>
#include <shv/rpchistory.h>

#include "getlog_param_keys.gperf.h"
#include "shvc_config.h"

const struct rpcdir rpchistory_fetch = {
	.name = "fetch",
	.param = "[Int, Int]",
	.result = "[{...}, ...]",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_span = {
	.name = "span",
	.param = "Null",
	.result = "[Int, Int, Int]",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_getlog = {
	.name = "getLog",
	.param = "{...}",
	.result = "[i{...}, ...]",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_BROWSE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_sync = {
	.name = "sync",
	.param = "Null",
	.result = "Null",
	.flags = 0,
	.access = RPCACCESS_SUPER_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_lastsync = {
	.name = "lastSync",
	.param = "Null",
	.result = "DateTime | Null",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

bool rpchistory_getlog_response_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head, int ref) {
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_TIMESTAMP);
	cp_pack_datetime(pack, head->datetime);

	if (ref >= 0) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_REF);
		cp_pack_int(pack, ref);
	}

	if (head->path && *head->path != '\0') {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_PATH);
		cp_pack_str(pack, head->path);
	}

	if (head->signal && *head->signal != '\0') {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_SIGNAL);
		cp_pack_str(pack, head->signal);
	}

	if (head->source && *head->source != '\0') {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_SOURCE);
		cp_pack_str(pack, head->source);
	}

	if (head->userid) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_USERID);
		cp_pack_str(pack, head->userid);
	}

	if (head->repeat) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_REPEAT);
		cp_pack_bool(pack, head->repeat);
	}

	cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_VALUE);
	return true;
}

bool rpchistory_getlog_response_pack_end(cp_pack_t pack) {
	cp_pack_container_end(pack);
	return true;
}

static bool unpack_value(cp_unpack_t unpack, struct cpitem *item,
	struct obstack *obstack, struct rpchistory_getlog_request *res, int key) {
	switch (key) {
		case K_SINCE:
			return cp_unpack_datetime(unpack, item, res->since);
		case K_UNTIL:
			return cp_unpack_datetime(unpack, item, res->until);
		case K_COUNT:
			switch (cp_unpack_type(unpack, item)) {
				case CPITEM_INT:
					return cpitem_extract_int(item, res->count);
				case CPITEM_NULL:
					res->count = UINT_MAX;
					return true;
				default:
					return false;
			}
		case K_SNAPSHOT:
			return cp_unpack_bool(unpack, item, res->snapshot);
		case K_RI:
			res->ri = cp_unpack_strdupo(unpack, item, obstack);
			if (res->ri == NULL)
				res->ri = "**:*";
			return true;
		default: /* skip values for unknown or unsupported keys */
			cp_unpack_skip(unpack, item);
			return true;
	}
}

struct rpchistory_getlog_request *rpchistory_getlog_request_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_type(unpack, item) != CPITEM_MAP)
		return NULL;

	struct rpchistory_getlog_request *res = obstack_alloc(obstack, sizeof(*res));
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	*res = (struct rpchistory_getlog_request){
		.since = {.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)},
		.until = {.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)},
		.count = -1,
		.snapshot = false,
		.ri = "**:*",
	};

	for_cp_unpack_map(unpack, item, key, 10) {
		const struct gperf_rpchistory_getlog_param_key_match *match =
			gperf_rpchistory_getlog_param_key(key, strlen(key));
		if (!match)
			cp_unpack_skip(unpack, item);
		else if (!unpack_value(unpack, item, obstack, res, match->key)) {
			obstack_free(obstack, res);
			return NULL;
		}
	}

	if (res->count == UINT_MAX && res->snapshot)
		res->count = 0;

	if (res->count > SHVC_GETLOG_LIMIT)
		res->count = SHVC_GETLOG_LIMIT;

	if (res->since.msecs >= res->until.msecs)
		res->snapshot = false;

	return res;
}

bool rpchistory_record_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head) {
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TYPE);
	cp_pack_int(pack, head->type);

	cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TIMESTAMP);
	cp_pack_datetime(pack, head->datetime);

	if (head->type == RPCHISTORY_RECORD_NORMAL ||
		head->type == RPCHISTORY_RECORD_KEEP) {
		if (head->path && *head->path != '\0') {
			cp_pack_int(pack, RPCHISTORY_FETCH_KEY_PATH);
			cp_pack_str(pack, head->path);
		}

		if (head->signal && *head->signal != '\0') {
			cp_pack_int(pack, RPCHISTORY_FETCH_KEY_SIGNAL);
			cp_pack_str(pack, head->signal);
		}

		if (head->source && *head->source != '\0') {
			cp_pack_int(pack, RPCHISTORY_FETCH_KEY_SOURCE);
			cp_pack_str(pack, head->source);
		}

		if (head->access > RPCACCESS_NONE) {
			cp_pack_int(pack, RPCHISTORY_FETCH_KEY_ACCESSLEVEL);
			cp_pack_int(pack, head->access);
		}

		if (head->userid) {
			cp_pack_int(pack, RPCHISTORY_FETCH_KEY_USERID);
			cp_pack_str(pack, head->userid);
		}

		if (head->repeat) {
			cp_pack_int(pack, RPCHISTORY_FETCH_KEY_REPEAT);
			cp_pack_bool(pack, head->repeat);
		}

		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_VALUE);
	}

	if (head->type == RPCHISTORY_RECORD_TIMEJUMP) {
		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TIMEJUMP);
		cp_pack_int(pack, head->timejump);
	}

	return true;
}

bool rpchistory_record_pack_end(cp_pack_t pack) {
	cp_pack_container_end(pack);
	return true;
}
