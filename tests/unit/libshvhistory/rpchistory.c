#include <shv/rpchistory.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpchistory"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"

#include "shvc_config.h"

static struct cpondir {
	struct rpchistory_record_head head;
	struct rpchistory_record_head head_unpack;
	const char *cpon;
} record_pairs_d[] = {
	{(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_NORMAL,
		 .datetime = {.msecs = 150000, .offutc = 60},
		 .path = NULL,
		 .signal = NULL,
		 .source = NULL,
		 .access = 0,
		 .userid = NULL,
		 .repeat = false},
		(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_NORMAL,
			.datetime = {.msecs = 150000, .offutc = 60},
			.path = "",
			.signal = "chng",
			.source = "get",
			.access = 0,
			.userid = NULL,
			.repeat = false},
		"i{0:1,1:d\"1970-01-01T00:02:30.000+01:00\",5:1u}"},
	{(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_NORMAL,
		 .datetime = {.msecs = 150000, .offutc = 60},
		 .path = "t_path",
		 .signal = "t_sig",
		 .source = "t_src",
		 .access = RPCACCESS_WRITE,
		 .userid = "0123",
		 .repeat = true},
		(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_NORMAL,
			.datetime = {.msecs = 150000, .offutc = 60},
			.path = "t_path",
			.signal = "t_sig",
			.source = "t_src",
			.access = RPCACCESS_WRITE,
			.userid = "0123",
			.repeat = true},
		"i{0:1,1:d\"1970-01-01T00:02:30.000+01:00\",2:\"t_path\",3:\"t_sig\",4:\"t_src\",6:16,7:\"0123\",8:true,5:1u}"},
	{(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_KEEP,
		 .datetime = {.msecs = 150000, .offutc = 60},
		 .path = NULL,
		 .signal = NULL,
		 .source = NULL,
		 .access = 0,
		 .userid = NULL,
		 .repeat = false},
		(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_KEEP,
			.datetime = {.msecs = 150000, .offutc = 60},
			.path = "",
			.signal = "chng",
			.source = "get",
			.access = 0,
			.userid = NULL,
			.repeat = false},
		"i{0:2,1:d\"1970-01-01T00:02:30.000+01:00\",5:1u}"},
	{(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_TIMEJUMP,
		 .datetime = {.msecs = 150000, .offutc = 60},
		 .timejump = 1500},
		(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_TIMEJUMP,
			.datetime = {.msecs = 150000, .offutc = 60},
			.timejump = 1500},
		"i{0:3,1:d\"1970-01-01T00:02:30.000+01:00\",60:1500}"},
	{(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_TIMEABIG,
		 .datetime = {.msecs = 150000, .offutc = 60}},
		(struct rpchistory_record_head){.type = RPCHISTORY_RECORD_TIMEABIG,
			.datetime = {.msecs = 150000, .offutc = 60}},
		"i{0:4,1:d\"1970-01-01T00:02:30.000+01:00\"}"},
};

static struct cpongetlog {
	int ref;
	struct rpchistory_record_head head;
	struct rpchistory_record_head head_unpack;
	const char *cpon;
} getlog_response_pairs_d[] = {
	{-1,
		(struct rpchistory_record_head){.datetime = {.msecs = 150000, .offutc = 60},
			.path = NULL,
			.signal = NULL,
			.source = NULL,
			.userid = NULL,
			.repeat = false},
		(struct rpchistory_record_head){.datetime = {.msecs = 150000, .offutc = 60},
			.path = "",
			.signal = "chng",
			.source = "get",
			.userid = NULL,
			.repeat = false},
		"i{1:d\"1970-01-01T00:02:30.000+01:00\",6:1u}"},
	{-1,
		(struct rpchistory_record_head){.datetime = {.msecs = 150000, .offutc = 60},
			.path = "t_path",
			.signal = "t_sig",
			.source = "t_src",
			.userid = "0123",
			.repeat = true},
		(struct rpchistory_record_head){.datetime = {.msecs = 150000, .offutc = 60},
			.path = "t_path",
			.signal = "t_sig",
			.source = "t_src",
			.userid = "0123",
			.repeat = true},
		"i{1:d\"1970-01-01T00:02:30.000+01:00\",3:\"t_path\",4:\"t_sig\",5:\"t_src\",7:\"0123\",8:true,6:1u}"},
	{0, (struct rpchistory_record_head){.datetime = {.msecs = 150000, .offutc = 60}},
		(struct rpchistory_record_head){.datetime = {.msecs = 150000, .offutc = 60}},
		"i{1:d\"1970-01-01T00:02:30.000+01:00\",2:0,6:1u}"},
};

static struct cpongetlog_request {
	struct rpchistory_getlog_request getlog;
	const char *cpon;
} getlog_request_pairs_d[] = {
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = 10,
		 .snapshot = false,
		 .ri = "path:method"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":10,\"snapshot\":false,\"ri\":\"path:method\"}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = SHVC_GETLOG_LIMIT,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":null,\"snapshot\":false,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = SHVC_GETLOG_LIMIT,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":10000,\"snapshot\":false,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = 0,
		 .snapshot = true,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":null,\"snapshot\":true,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = 15,
		 .snapshot = true,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":15,\"snapshot\":true,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){
		 .since = {.msecs = 157766550000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = 15,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"1975-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":15,\"snapshot\":true,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = -2,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":\"invalid\",\"snapshot\":false,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = -2,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"invalid\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":10,\"snapshot\":false,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = -2,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"invalid\",\"count\":\"invalid\",\"snapshot\":false,\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.since = {.msecs = 150000, .offutc = 60},
		 .until = {.msecs = 300000, .offutc = 60},
		 .count = -2,
		 .snapshot = false,
		 .ri = "**:*"},
		"{\"since\":d\"1970-01-01T00:02:30.000+01:00\",\"until\":d\"1970-01-01T00:05:00.000+01:00\",\"count\":10,\"snapshot\":\"invalid\",\"ri\":null,\"invalid\":null}"},
	{(struct rpchistory_getlog_request){.count = -2}, "154u"},
};


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, record_packer, record_pairs_d) {
	rpchistory_record_pack_begin(packstream_pack, &_d.head);
	if ((_d.head.type == RPCHISTORY_RECORD_KEEP) ||
		(_d.head.type == RPCHISTORY_RECORD_NORMAL))
		cp_pack_uint(packstream_pack, 1);
	rpchistory_record_pack_end(packstream_pack);
	ck_assert_packstr(_d.cpon);
}

ARRAY_TEST(pack, getlog_packer, getlog_response_pairs_d) {
	rpchistory_getlog_response_pack_begin(packstream_pack, &_d.head, _d.ref);
	cp_pack_uint(packstream_pack, 1);
	rpchistory_getlog_response_pack_end(packstream_pack);
	ck_assert_packstr(_d.cpon);
}

TEST_CASE(unpack) {}

static void getlog_request_unpack(struct cpongetlog_request _d) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);
	struct obstack obstack;
	obstack_init(&obstack);

	struct rpchistory_getlog_request *getlog =
		rpchistory_getlog_request_unpack(unpack, &item, &obstack);
	if (_d.getlog.count == -2) {
		ck_assert_ptr_null(getlog);
		goto test_end;
	}

	ck_assert_ptr_nonnull(getlog);

	ck_assert_int_eq(getlog->since.msecs, _d.getlog.since.msecs);
	ck_assert_int_eq(getlog->since.offutc, _d.getlog.since.offutc);
	ck_assert_int_eq(getlog->until.msecs, _d.getlog.until.msecs);
	ck_assert_int_eq(getlog->until.offutc, _d.getlog.until.offutc);
	ck_assert_int_eq(getlog->count, _d.getlog.count);
	ck_assert_int_eq(getlog->snapshot, _d.getlog.snapshot);
	ck_assert_str_eq(getlog->ri, _d.getlog.ri);

test_end:
	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}

ARRAY_TEST(unpack, unpacker, getlog_request_pairs_d) {
	getlog_request_unpack(_d);
}
