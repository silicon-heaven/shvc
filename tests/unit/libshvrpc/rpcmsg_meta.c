#include <shv/rpcmsg.h>
#include <stdlib.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpcmsg_meta"
#include <check_suite.h>
#include "unpack.h"
#include "item.h"


TEST_CASE(all) {}

static const struct {
	const char *str;
	const struct rpcmsg_meta meta;
} unpack_obstack_d[] = {
	{"<>null", (struct rpcmsg_meta){.request_id = INT64_MIN}},
	{"<8:42,9:\".app\",10:\"ping\",13:\"some,dev,acc\">null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 42,
			.path = ".app",
			.method = "ping",
			.access_grant = RPCMSG_ACC_DEVEL,
		}},
	{"<1:1,8:24u,11:[0,3]>null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_RESPONSE,
			.request_id = 24,
			.cids = {.ptr = (void *)(const uint8_t[]){0x88, 0x40, 0x43, 0xff},
				.siz = 4},
		}},
};
ARRAY_TEST(all, unpack_obstack) {
	cp_unpack_t unpack = unpack_cpon(_d.str);
	struct cpitem item = (struct cpitem){};
	struct rpcmsg_meta meta = (struct rpcmsg_meta){};
	struct obstack obstack;
	obstack_init(&obstack);

	ck_assert(rpcmsg_meta_unpack(unpack, &item, &meta, NULL, &obstack));
	ck_assert_int_eq(meta.type, _d.meta.type);
	ck_assert_int_eq(meta.request_id, _d.meta.request_id);
	ck_assert_pstr_eq(meta.path, _d.meta.path);
	ck_assert_pstr_eq(meta.method, _d.meta.method);
	ck_assert_int_eq(meta.access_grant, _d.meta.access_grant);
	ck_assert_int_eq(meta.cids.siz, _d.meta.cids.siz);
	ck_assert_mem_eq(meta.cids.ptr, _d.meta.cids.ptr, _d.meta.cids.siz);
	cp_unpack(unpack, &item);
	ck_assert_item_type(item, CP_ITEM_NULL);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST
