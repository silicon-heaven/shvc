#include <stdlib.h>
#include <shv/rpchistory.h>

#include "shv/cp_unpack.h"
#include "shvc_config.h"

const struct rpcdir rpchistory_fetch = {
	.name = "fetch",
	.param = "[i:offset,i(0,):count]",
	.result = "!historyRecords",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_span = {
	.name = "span",
	.result = "[i:smallest,i:biggest,i(1,):span]",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_datespan = {
	.name = "dateSpan",
	.param = "!historyDateSpanP",
	.result = "!historyDateSpanR",
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_getlog = {
	.name = "getLog",
	.param = "!getLogP",
	.result = "!getLogR",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_BROWSE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_getsnapshot = {
	.name = "getSnapshot",
	.param = "!getSnapshotP",
	.result = "!getLogR",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_BROWSE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_sync = {
	.name = "sync",
	.flags = 0,
	.access = RPCACCESS_SUPER_SERVICE,
	.signals_cnt = 0,
};

const struct rpcdir rpchistory_lastsync = {
	.name = "lastSync",
	.result = "t|n",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

bool rpchistory_getlog_response_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head) {
	cp_pack_imap_begin(pack);

	if (head->datetime.msecs != INT64_MAX) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_TIMESTAMP);
		cp_pack_datetime(pack, head->datetime);
	}

	if (head->ref >= 0) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_REF);
		cp_pack_int(pack, head->ref);
	} else {
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
	}

	if (head->userid) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_USERID);
		cp_pack_str(pack, head->userid);
	}

	if (head->repeat) {
		cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_REPEAT);
		cp_pack_bool(pack, head->repeat);
	}

	return cp_pack_int(pack, RPCHISTORY_GETLOG_KEY_VALUE);
}

bool rpchistory_getlog_response_pack_end(cp_pack_t pack) {
	return cp_pack_container_end(pack);
}

static bool unpack_value(cp_unpack_t unpack, struct cpitem *item,
	struct obstack *obstack, struct rpchistory_getlog_request *res, int key) {
	switch (key) {
		case RPCHISTORY_GETLOG_REQ_KEY_SINCE:
			return cp_unpack_datetime(unpack, item, res->since);
		case RPCHISTORY_GETLOG_REQ_KEY_UNTIL:
			return cp_unpack_datetime(unpack, item, res->until);
		case RPCHISTORY_GETLOG_REQ_KEY_COUNT:
			switch (cp_unpack_type(unpack, item)) {
				case CPITEM_INT:
					return cpitem_extract_int(item, res->count);
				case CPITEM_NULL:
					res->count = UINT_MAX;
					return true;
				default:
					return false;
			}
		case RPCHISTORY_GETLOG_REQ_KEY_RI:
			res->ri = cp_unpack_strdupo(unpack, item, obstack);
			if (res->ri == NULL)
				res->ri = "**:*";
			return true;
		default:
			return false;
	}
}

struct rpchistory_getlog_request *rpchistory_getlog_request_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return NULL;

	struct rpchistory_getlog_request *res = obstack_alloc(obstack, sizeof(*res));
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	*res = (struct rpchistory_getlog_request){
		.since = {.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)},
		.until = {.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)},
		.count = -1,
		.ri = "**:*",
	};

	for_cp_unpack_imap(unpack, item, key) {
		if (!unpack_value(unpack, item, obstack, res, key)) {
			obstack_free(obstack, res);
			return NULL;
		}
	}

	if (res->count > SHVC_GETLOG_LIMIT)
		res->count = SHVC_GETLOG_LIMIT;

	return res;
}

struct rpchistory_getsnapshot_request *rpchistory_getsnapshot_request_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return NULL;

	struct rpchistory_getsnapshot_request *res =
		obstack_alloc(obstack, sizeof *res);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	*res = (struct rpchistory_getsnapshot_request){
		.time = {.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)},
		.ri = "**:*",
	};

	for_cp_unpack_imap(unpack, item, key) {
		switch (key) {
			case RPCHISTORY_GETSNAPSHOT_REQ_KEY_TIME:
				if (!cp_unpack_datetime(unpack, item, res->time))
					goto getsnapshot_err;
				break;
			case RPCHISTORY_GETSNAPSHOT_REQ_KEY_RI:
				res->ri = cp_unpack_strdupo(unpack, item, obstack);
				if (res->ri == NULL)
					res->ri = "**:*";
				break;
			default:
				goto getsnapshot_err;
		}
	}

	return res;

getsnapshot_err:
	obstack_free(obstack, res);
	return NULL;
}

bool rpchistory_record_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head) {
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TYPE);
	cp_pack_int(pack, head->type);

	cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TIMESTAMP);
	bool res = cp_pack_datetime(pack, head->datetime);

	if (head->id >= 0) {
		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_ID);
		res = cp_pack_int(pack, head->id);
	}

	if (head->ref >= 0) {
		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_REF);
		cp_pack_int(pack, head->ref);
	}

	if (head->idref >= 0) {
		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_IDREF);
		cp_pack_int(pack, head->idref);
	}

	if (head->type == RPCHISTORY_RECORD_NORMAL ||
		head->type == RPCHISTORY_RECORD_KEEP) {
		if (head->ref < 0) {
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

		res = cp_pack_int(pack, RPCHISTORY_FETCH_KEY_VALUE);
	} else if (head->type == RPCHISTORY_RECORD_TIMEJUMP) {
		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TIMEJUMP);
		res = cp_pack_int(pack, head->timejump);
	}

	return res;
}

bool rpchistory_record_pack_end(cp_pack_t pack) {
	return cp_pack_container_end(pack);
}

bool rpchistory_file_record_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head) {
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCHISTORY_FILE_KEY_DATETIME);
	if (head->datetime.msecs == INT64_MAX)
		cp_pack_null(pack);
	else
		cp_pack_datetime(pack, head->datetime);

	if (head->path && *head->path != '\0') {
		cp_pack_int(pack, RPCHISTORY_FILE_KEY_PATH);
		cp_pack_str(pack, head->path);
	}

	if (head->signal && *head->signal != '\0') {
		cp_pack_int(pack, RPCHISTORY_FILE_KEY_SIGNAL);
		cp_pack_str(pack, head->signal);
	}

	if (head->source && *head->source != '\0') {
		cp_pack_int(pack, RPCHISTORY_FILE_KEY_SOURCE);
		cp_pack_str(pack, head->source);
	}

	if (head->access != RPCACCESS_READ && head->access != RPCACCESS_NONE) {
		cp_pack_int(pack, RPCHISTORY_FILE_KEY_ACCESS);
		cp_pack_int(pack, head->access);
	}

	if (head->userid) {
		cp_pack_int(pack, RPCHISTORY_FILE_KEY_USERID);
		cp_pack_str(pack, head->userid);
	}

	if (head->repeat) {
		cp_pack_int(pack, RPCHISTORY_FILE_KEY_REPEAT);
		cp_pack_bool(pack, head->repeat);
	}

	return cp_pack_int(pack, RPCHISTORY_FILE_KEY_VALUE);
}

bool rpchistory_file_record_pack_end(cp_pack_t pack) {
	return cp_pack_container_end(pack);
}

bool rpchistory_datespan_response_pack(
	cp_pack_t pack, struct rpchistory_datespan_response *res) {
	cp_pack_list_begin(pack);
	cp_pack_int(pack, res->since);
	cp_pack_int(pack, res->until);
	cp_pack_datetime(pack, res->until_date);
	return cp_pack_container_end(pack);
}

struct rpchistory_datespan_request *rpchistory_datespan_request_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return NULL;

	struct rpchistory_datespan_request *res = obstack_alloc(obstack, sizeof *res);
	struct timespec ts, mon;
	clock_gettime(CLOCK_REALTIME, &ts);
	clock_gettime(CLOCK_MONOTONIC, &mon);
	*res = (struct rpchistory_datespan_request){
		.since = {.msecs = 0},
		.until = {.msecs = INT64_MAX},
		.now = {.msecs = INT64_MAX},
	};

	for_cp_unpack_imap(unpack, item, key) {
		switch (key) {
			case RPCHISTORY_DATESPAN_REQ_KEY_SINCE:
				if (!cp_unpack_datetime(unpack, item, res->since))
					goto datespan_err;
				break;
			case RPCHISTORY_DATESPAN_REQ_KEY_UNTIL:
				if (!cp_unpack_datetime(unpack, item, res->until))
					goto datespan_err;
				break;
			case RPCHISTORY_DATESPAN_REQ_KEY_NOW:
				if (!cp_unpack_datetime(unpack, item, res->now))
					goto datespan_err;
				break;
			default:
				goto datespan_err;
		}
	}

	if (res->now.msecs != INT64_MAX && res->until.msecs == INT64_MAX) {
		res->until = res->now;
	}

	if (res->until.msecs == INT64_MAX) {
		res->until.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
	}

	if (res->until.msecs < res->since.msecs) {
		goto datespan_err;
	}

	return res;

datespan_err:
	obstack_free(obstack, res);
	return NULL;
}
