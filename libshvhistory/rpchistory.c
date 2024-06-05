#include <stdlib.h>
#include <shv/rpchistory.h>

struct rpcdir rpchistory_fetch = {
	.name = "fetch",
	.param = "[Int, Int]",
	.result = "[{...}, ...]",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

struct rpcdir rpchistory_span = {
	.name = "span",
	.param = "Null",
	.result = "[Int, Int, Int]",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

struct rpcdir rpchistory_getlog = {
	.name = "getLog",
	.param = "{...}",
	.result = "[i{...}, ...]",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_BROWSE,
	.signals_cnt = 0,
};

struct rpcdir rpchistory_sync = {
	.name = "sync",
	.param = "Null",
	.result = "Null",
	.flags = 0,
	.access = RPCACCESS_SUPER_SERVICE,
	.signals_cnt = 0,
};

struct rpcdir rpchistory_lastsync = {
	.name = "lastSync",
	.param = "Null",
	.result = "DateTime | Null",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_SERVICE,
	.signals_cnt = 0,
};

bool rpchistory_record_pack_begin(
	cp_pack_t pack, struct rpchistory_record_head *head) {
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TYPE);
	cp_pack_int(pack, head->type);

	cp_pack_int(pack, RPCHISTORY_FETCH_KEY_TIMESTAMP);
	cp_pack_datetime(pack, head->datetime);

	if (head->type == RPCHISTORY_RECORD_NORMAL ||
		head->type == RPCHISTORY_RECORD_KEEP) {
		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_PATH);
		if (head->path == NULL)
			head->path = "";
		cp_pack_str(pack, head->path);

		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_SIGNAL);
		if (head->signal == NULL)
			head->signal = "chng";
		cp_pack_str(pack, head->signal);

		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_SOURCE);
		if (head->source == NULL)
			head->source = "get";
		cp_pack_str(pack, head->source);

		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_ACCESSLEVEL);
		if (head->access == RPCACCESS_NONE)
			head->access = RPCACCESS_READ;
		cp_pack_int(pack, head->access);

		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_USERID);
		if (head->userid == NULL)
			cp_pack_null(pack);
		else
			cp_pack_str(pack, head->userid);

		cp_pack_int(pack, RPCHISTORY_FETCH_KEY_REPEAT);
		cp_pack_bool(pack, head->repeat);

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
