#include <shv/rpcerror.h>
#include <stdlib.h>

#define SUITE "rpcerror"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"


static const struct cponerr {
	rpcerrno_t errno;
	const char *errmsg;
	const char *cpon;
} pairs_d[] = {
	{RPCERR_INVALID_PARAMS, "Use Int", "i{1:3,2:\"Use Int\"}"},
};


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, packer, pairs_d) {
	rpcerror_pack(packstream_pack, _d.errno, _d.errmsg);
	ck_assert_packstr(_d.cpon);
}


TEST_CASE(unpack) {}

static void unpacker_test(struct cponerr _d) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);

	rpcerrno_t errno;
	char *errmsg;
	ck_assert(rpcerror_unpack(unpack, &item, &errno, &errmsg));
	ck_assert_int_eq(errno, _d.errno);
	ck_assert_pstr_eq(errmsg, _d.errmsg);

	free(errmsg);
	unpack_free(unpack);
}

ARRAY_TEST(unpack, unpacker, pairs_d) {
	unpacker_test(_d);
}
static struct cponerr unpacker_comp_d[] = {
	{RPCERR_PARSE_ERR, NULL, "i{1:5u,63:[]}"},
	{RPCERR_PARSE_ERR, "Ups", "i{2:\"Ups\",1:5}"},
};
ARRAY_TEST(unpack, unpacker_comp) {
	unpacker_test(_d);
}

static const char *const unpack_invalid_d[] = {
	"i{}",
	"{}",
	"[]",
	"i{2:\"some\",1:\"foo\"}",
	"i{2:5}",
};
ARRAY_TEST(unpack, unpack_invalid) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d);
	rpcerrno_t errno;
	ck_assert(!rpcerror_unpack(unpack, &item, &errno, NULL));
	unpack_free(unpack);
}
