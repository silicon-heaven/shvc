#include <shv/rpcalerts.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpcalerts"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"

static struct cpondir {
	struct rpcalerts alerts;
	const char *cpon;
} stat_pairs_d[] = {
	{(struct rpcalerts){.time = 150000, .level = 15, .id = "RPCDEVICE_ALERT"},
		"i{0:d\"1970-01-01T00:02:30.000Z\",1:15,2:\"RPCDEVICE_ALERT\"}"},
};


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, stat_packer, stat_pairs_d) {
	rpcalerts_pack(packstream_pack, &_d.alerts);
	ck_assert_packstr(_d.cpon);
}

TEST_CASE(unpack) {}

static void stat_unpack_test(struct cpondir _d) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);
	struct obstack obstack;
	obstack_init(&obstack);

	struct rpcalerts *alerts = rpcalerts_unpack(unpack, &item, &obstack);
	ck_assert_ptr_nonnull(alerts);

	ck_assert_int_eq(alerts->time, _d.alerts.time);
	ck_assert_int_eq(alerts->level, _d.alerts.level);
	ck_assert_str_eq(alerts->id, _d.alerts.id);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}

ARRAY_TEST(unpack, unpacker, stat_pairs_d) {
	stat_unpack_test(_d);
}
