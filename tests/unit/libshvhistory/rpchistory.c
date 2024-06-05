#include <shv/rpchistory.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpchistory"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"

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
		"i{0:1,1:d\"1970-01-01T00:02:30.000+01:00\",2:\"\",3:\"chng\",4:\"get\",6:8,7:null,8:false,5:1u}"},
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
		"i{0:2,1:d\"1970-01-01T00:02:30.000+01:00\",2:\"\",3:\"chng\",4:\"get\",6:8,7:null,8:false,5:1u}"},
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


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, record_packer, record_pairs_d) {
	rpchistory_record_pack_begin(packstream_pack, &_d.head);
	if ((_d.head.type == RPCHISTORY_RECORD_KEEP) ||
		(_d.head.type == RPCHISTORY_RECORD_NORMAL))
		cp_pack_uint(packstream_pack, 1);
	rpchistory_record_pack_end(packstream_pack);
	ck_assert_packstr(_d.cpon);
}
