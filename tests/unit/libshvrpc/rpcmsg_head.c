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
} unpack_d[] = {
	{
		"<8:3,10:\"ls\">i{1:null}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 3,
			.path = "",
			.method = "ls",
			.access = RPCACCESS_ADMIN,
		},
	},
	{
		"<8:42,10:\"get\">i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 42,
			.path = "",
			.method = "get",
			.access = RPCACCESS_ADMIN,
		},
	},
	{
		"<1:1,2:0,8:42,9:\".app\",10:\"ping\",14:\"some,dev,acc\",17:56>i{1:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 42,
			.path = ".app",
			.method = "ping",
			.access = RPCACCESS_DEVEL,
			.access_granted = "some,acc",
		},
	},
	{
		"<8:1,9:\".broker/app\",10:\"ping\">i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.access = RPCACCESS_ADMIN,
			.request_id = 1,
			.path = ".broker/app",
			.method = "ping",
		},
	},
	{
		"<8:24,11:[0,3],16:\"user\">i{2:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_RESPONSE,
			.request_id = 24,
			.user_id = "user",
			.cids = (long long[]){0, 3},
			.cids_cnt = 2,
		},
	},
	{
		"<8:4>i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_RESPONSE,
			.request_id = 4,
		},
	},
	{
		"<8:86>i{3:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_ERROR,
			.request_id = 86,
		},
	},
	{
		"<>i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.path = "",
			.signal = "chng",
			.source = "get",
			.access = RPCACCESS_READ,
		},
	},
	{
		"<10:\"foo\">i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.path = "",
			.signal = "foo",
			.source = "get",
			.access = RPCACCESS_READ,
		},
	},
	{
		"<9:\"value\">i{1:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.path = "value",
			.method = "chng",
			.source = "get",
			.access = RPCACCESS_READ,
		},
	},
	{
		"<9:\"signal\",10:\"fchng\",19:\"data\",20:true>i{}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.path = "signal",
			.method = "fchng",
			.source = "data",
			.repeat = true,
			.access = RPCACCESS_READ,
		},
	},
	{
		"<9:\"value\",12:[4,8],13:\"foo\",17:32>i{1:null",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_SIGNAL,
			.path = "value",
			.method = "chng",
			.source = "get",
			.access = RPCACCESS_CONFIG,
			.extra =
				&(struct rpcmsg_meta_extra){
					.key = 12,
					.ptr = (uint8_t[]){0x88, 0x44, 0x48, 0xff},
					.siz = 4,
					.next =
						&(struct rpcmsg_meta_extra){
							.key = 13,
							.ptr = (uint8_t[]){0x8e, 'f', 'o', 'o', '\0'},
							.siz = 5,
						},
				},
		},
	},
	{
		"<1:1,8:175,9:\"\",10:\"ls\",11:4,14:\"dev\",\"unsupported\":53>i{1:null}",
		(struct rpcmsg_meta){
			.type = RPCMSG_T_REQUEST,
			.request_id = 175,
			.path = "",
			.method = "ls",
			.cids = (long long[]){4},
			.cids_cnt = 1,
			.access = RPCACCESS_DEVEL,
		},
	},
};
ARRAY_TEST(all, unpack) {
	cp_unpack_t unpack = unpack_cpon(_d.str);
	struct cpitem item = (struct cpitem){};
	struct rpcmsg_meta meta = (struct rpcmsg_meta){};
	struct rpcmsg_meta_limits limits = rpcmsg_meta_limits_default;
	limits.extra = true;
	struct obstack obstack;
	obstack_init(&obstack);

	ck_assert(rpcmsg_head_unpack(unpack, &item, &meta, &limits, &obstack));

	ck_assert_int_eq(meta.type, _d.meta.type);
	ck_assert_int_eq(meta.request_id, _d.meta.request_id);
	ck_assert_int_eq(meta.access, _d.meta.access);
	ck_assert_pstr_eq(meta.access_granted, _d.meta.access_granted);
	ck_assert_pstr_eq(meta.path, _d.meta.path);
	ck_assert_pstr_eq(meta.method, _d.meta.method); /* Also covers signal */
	ck_assert_pstr_eq(meta.source, _d.meta.source);
	ck_assert(meta.repeat == _d.meta.repeat);
	ck_assert_pstr_eq(meta.user_id, _d.meta.user_id);
	ck_assert_int_eq(meta.cids_cnt, _d.meta.cids_cnt);
	ck_assert_mem_eq(meta.cids, _d.meta.cids, _d.meta.cids_cnt * sizeof(long long));
	struct rpcmsg_meta_extra *e = meta.extra;
	for (struct rpcmsg_meta_extra *re = _d.meta.extra; re; re = re->next) {
		ck_assert_ptr_nonnull(e);
		ck_assert_int_eq(e->key, re->key);
		ck_assert_int_eq(e->siz, re->siz);
		ck_assert_mem_eq(e->ptr, re->ptr, re->siz);
		e = e->next;
	}
	ck_assert_ptr_null(e);

	if (rpcmsg_has_param(&item)) {
		cp_unpack(unpack, &item);
		ck_assert_item_type(item, CPITEM_NULL);
	} /* Otherwise this was all that is in the message */

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST

static const char *const unpack_invalid_d[] = {
	"<>{}",
	"<8:\"foo\">i{}",
	"<8:42>i{1:null}",
	"<>i{2:null}",
	"<1:2>i{}",
	"<1:null>i{}",
	"<2:null>i{}",
	"<17:\"bws\">i{}",
	"<63:{},20:5>i{}",
	"<11:\"foo\">i{}",
	"<11:[\"foo\"]>i{}",
	"i{}",
	"<8:42,9:\"foo\",10:\"get\">i{2:",
};
ARRAY_TEST(all, unpack_invalid) {
	cp_unpack_t unpack = unpack_cpon(_d);
	struct cpitem item = (struct cpitem){};
	struct rpcmsg_meta meta = (struct rpcmsg_meta){};
	struct obstack obstack;
	obstack_init(&obstack);
	void *obase = obstack_base(&obstack);

	ck_assert(!rpcmsg_head_unpack(unpack, &item, &meta, NULL, &obstack));
	ck_assert_int_eq(meta.type, RPCMSG_T_INVALID);
	ck_assert_ptr_eq(obstack_base(&obstack), obase);

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
END_TEST

// TODO check that we adhere to the limits
