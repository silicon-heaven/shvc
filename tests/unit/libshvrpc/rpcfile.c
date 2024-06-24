#include <shv/rpcfile.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpcfile"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"

static struct cpondir {
	struct rpcfile_stat_s stats;
	const char *cpon;
} stat_pairs_d[] = {
	{(struct rpcfile_stat_s){
		 .type = 0,
		 .size = 2048,
		 .page_size = 512,
		 .access_time = {.msecs = 150000, .offutc = 60},
		 .mod_time = {.msecs = 0, .offutc = 0},
		 .max_write = 128,
	 },
		"i{0:0,1:2048,2:512,3:d\"1970-01-01T00:02:30.000+01:00\",4:d\"1970-01-01T00:00:00.000Z\",5:128}"},
};


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, stat_packer, stat_pairs_d) {
	rpcfile_stat_pack(packstream_pack, &_d.stats);
	ck_assert_packstr(_d.cpon);
}

TEST_CASE(unpack) {}

static void stat_unpack_test(struct cpondir _d) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);
	struct obstack obstack;
	obstack_init(&obstack);

	struct rpcfile_stat_s *stats = rpcfile_stat_unpack(unpack, &item, &obstack);
	ck_assert_ptr_nonnull(stats);

	ck_assert_int_eq(stats->type, _d.stats.type);
	ck_assert_int_eq(stats->size, _d.stats.size);
	ck_assert_int_eq(stats->page_size, _d.stats.page_size);
	ck_assert_int_eq(stats->access_time.msecs, _d.stats.access_time.msecs);
	ck_assert_int_eq(stats->access_time.offutc, _d.stats.access_time.offutc);

	ck_assert_int_eq(stats->mod_time.msecs, _d.stats.mod_time.msecs);
	ck_assert_int_eq(stats->mod_time.offutc, _d.stats.mod_time.offutc);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}

ARRAY_TEST(unpack, unpacker, stat_pairs_d) {
	stat_unpack_test(_d);
}
