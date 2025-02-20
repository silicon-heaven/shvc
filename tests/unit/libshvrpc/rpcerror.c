#include <stdlib.h>
#include <shv/rpcerror.h>

#define SUITE "rpcerror"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"


static const struct cponerr {
	rpcerrno_t errnum;
	const char *errmsg;
	const char *cpon;
} pairs_d[] = {
	{RPCERR_INVALID_PARAM, "Use Int", "i{1:3,2:\"Use Int\"}"},
};


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, packer, pairs_d) {
	rpcerror_pack(packstream_pack, _d.errnum, _d.errmsg);
	ck_assert_packstr(_d.cpon);
}


TEST_CASE(unpack) {}

static void unpacker_test(struct cponerr _d) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);

	rpcerrno_t errnum;
	char *errmsg;
	ck_assert(rpcerror_unpack(unpack, &item, &errnum, &errmsg));
	ck_assert_int_eq(errnum, _d.errnum);
	ck_assert_pstr_eq(errmsg, _d.errmsg);

	free(errmsg);
	unpack_free(unpack);
}

ARRAY_TEST(unpack, unpacker, pairs_d) {
	unpacker_test(_d);
}
static struct cponerr unpacker_comp_d[] = {
	{RPCERR_USR1, NULL, "i{1:6u,63:[]}"},
	{RPCERR_USR2, "Ups", "i{2:\"Ups\",1:7}"},
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
	rpcerrno_t errnum;
	ck_assert(!rpcerror_unpack(unpack, &item, &errnum, NULL));
	unpack_free(unpack);
}
