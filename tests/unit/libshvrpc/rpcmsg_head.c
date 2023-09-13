#include "check.h"
#include "shv/cp.h"
#include <shv/rpcmsg.h>
#include <stdlib.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpcmsg_head"
#include <check_suite.h>
#include "unpack.h"
#include "item.h"


TEST_CASE(all) {}

static const struct {
	const char *str;
	const struct rpcmsg_meta meta;
	bool valid;
} unpack_obstack_d[] = {
	{
		"<>i{1:null}",
		(struct rpcmsg_meta){.type = RPCMSG_T_INVALID, .request_id = INT64_MIN},
		false,
	},
	{
		"<8:42,9:\".app\",10:\"ping\",13:\"some,dev,acc\">i{1:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 42,
			.path = ".app",
			.method = "ping",
			.access_grant = RPCMSG_ACC_DEVEL,
		},
		true,
	},
	{
		"<8:1,9:\".broker/app\",10:\"ping\">i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 1,
			.path = ".broker/app",
			.method = "ping",
		},
		true,
	},
	{
		"<1:1,8:24,11:[0,3]>i{2:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_RESPONSE,
			.request_id = 24,
			.cids =
				{
					.ptr = (void *)(const uint8_t[]){0x88, 0x40, 0x43, 0xff},
					.siz = 4,
				},
		},
		true,
	},
	{
		"<1:1,8:4>i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_RESPONSE,
			.request_id = 4,
		},
		true,
	},
	{
		"<1:1,8:86>i{3:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_ERROR,
			.request_id = 86,
		},
		true,
	},
	{
		"<1:1,9:\"value\",10:\"chng\">i{1:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.request_id = INT64_MIN,
			.path = "value",
			.method = "chng",
		},
		true,
	},
	{
		"<1:1,9:\"signal\",10:\"chng\">i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.request_id = INT64_MIN,
			.path = "signal",
			.method = "chng",
		},
		true,
	},
	{
		"<1:1,8:175,9:\"\",10:\"ls\",11:4,13:\"dev\">i{1:null}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 175,
			.path = "",
			.method = "ls",
			.cids = {.ptr = (void *)(const uint8_t[]){0x44}, .siz = 1},
			.access_grant = RPCMSG_ACC_DEVEL,
		},
		true,
	},
};
ARRAY_TEST(all, unpack_obstack) {
	cp_unpack_t unpack = unpack_cpon(_d.str);
	struct cpitem item = (struct cpitem){};
	struct rpcmsg_meta meta = (struct rpcmsg_meta){};
	struct obstack obstack;
	obstack_init(&obstack);

	ck_assert(rpcmsg_head_unpack(unpack, &item, &meta, NULL, &obstack) == _d.valid);
	ck_assert_int_eq(meta.type, _d.meta.type);
	ck_assert_int_eq(meta.request_id, _d.meta.request_id);
	ck_assert_pstr_eq(meta.path, _d.meta.path);
	ck_assert_pstr_eq(meta.method, _d.meta.method);
	ck_assert_int_eq(meta.access_grant, _d.meta.access_grant);
	ck_assert_int_eq(meta.cids.siz, _d.meta.cids.siz);
	ck_assert_mem_eq(meta.cids.ptr, _d.meta.cids.ptr, _d.meta.cids.siz);
	if (item.type != CPITEM_CONTAINER_END) {
		cp_unpack(unpack, &item);
		ck_assert_item_type(item, CPITEM_NULL);
	} /* Otherwise this was all that is in the message */

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST


TEST(all, unpack_extra) {
	static const char *const str = "<1:1,8:4,41:8,42:\"foo\",43:[1,2,3]>i{}";
	cp_unpack_t unpack = unpack_cpon(str);
	struct cpitem item = (struct cpitem){};
	struct rpcmsg_meta meta = (struct rpcmsg_meta){};
	struct rpcmsg_meta_limits limits = (struct rpcmsg_meta_limits){.extra = true};
	struct obstack obstack;
	obstack_init(&obstack);

	ck_assert(rpcmsg_head_unpack(unpack, &item, &meta, &limits, &obstack));
	ck_assert_int_eq(meta.type, RPCMSG_T_RESPONSE);
	ck_assert_int_eq(meta.request_id, 4);
	ck_assert_ptr_nonnull(meta.extra);
	ck_assert_int_eq(meta.extra->key, 41);
	ck_assert_bdata(meta.extra->ptr.ptr, meta.extra->ptr.siz, B(0x48));
	ck_assert_ptr_nonnull(meta.extra->next);
	ck_assert_int_eq(meta.extra->next->key, 42);
	ck_assert_bdata(meta.extra->next->ptr.ptr, meta.extra->next->ptr.siz,
		B(0x8e, 'f', 'o', 'o', '\0'));
	ck_assert_ptr_nonnull(meta.extra->next->next);
	ck_assert_int_eq(meta.extra->next->next->key, 43);
	ck_assert_bdata(meta.extra->next->next->ptr.ptr,
		meta.extra->next->next->ptr.siz, B(0x88, 0x41, 0x42, 0x43, 0xff));
	ck_assert_ptr_null(meta.extra->next->next->next);
	ck_assert_int_eq(item.type, CPITEM_CONTAINER_END);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST
