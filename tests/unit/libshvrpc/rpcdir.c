#include <shv/rpcdir.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define SUITE "rpcdir"
#include <check_suite.h>
#include "packstream.h"
#include "unpack.h"


static const struct cpondir {
	struct rpcdir dir;
	const char *cpon;
} pairs_d[] = {
	{(struct rpcdir){.name = "name", .access = RPCACCESS_READ}, "i{1:\"name\",5:8}"},
	{(struct rpcdir){
		 .name = "get",
		 .param = "Int",
		 .result = "String",
		 .flags = RPCDIR_F_GETTER,
		 .access = RPCACCESS_READ,
		 .signals =
			 (const struct rpcdirsig[]){
				 {.name = "chng"},
				 {.name = "maint", .param = "Bool"},
			 },
		 .signals_cnt = 2,
	 },
		"i{1:\"get\",2:2,3:\"Int\",4:\"String\",5:8,6:{\"chng\":null,\"maint\":\"Bool\"}}"},
};


TEST_CASE(pack, setup_packstream_pack_cpon, teardown_packstream_pack) {}

ARRAY_TEST(pack, packer, pairs_d) {
	rpcdir_pack(packstream_pack, &_d.dir);
	ck_assert_packstr(_d.cpon);
}

TEST(pack, pack_ls) {
	rpcdir_pack(packstream_pack, &rpcdir_ls);
	ck_assert_packstr(
		"i{1:\"ls\",3:\"ils\",4:\"ols\",5:1,6:{\"lsmod\":\"olsmod\"}}");
}

TEST(pack, pack_dir) {
	rpcdir_pack(packstream_pack, &rpcdir_dir);
	ck_assert_packstr("i{1:\"dir\",3:\"idir\",4:\"odir\",5:1}");
}


TEST_CASE(unpack) {}

static void unpacker_test(struct cpondir _d) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d.cpon);
	struct obstack obstack;
	obstack_init(&obstack);

	struct rpcdir *dir = rpcdir_unpack(unpack, &item, &obstack);
	ck_assert_ptr_nonnull(dir);
	ck_assert_str_eq(dir->name, _d.dir.name);
	ck_assert_pstr_eq(dir->param, _d.dir.param);
	ck_assert_pstr_eq(dir->result, _d.dir.result);
	ck_assert_int_eq(dir->flags, _d.dir.flags);
	ck_assert_int_eq(dir->signals_cnt, _d.dir.signals_cnt);
	for (size_t i = 0; i < _d.dir.signals_cnt; i++) {
		ck_assert_pstr_eq(dir->signals[i].name, _d.dir.signals[i].name);
		ck_assert_pstr_eq(dir->signals[i].param, _d.dir.signals[i].param);
	}

	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}

ARRAY_TEST(unpack, unpacker, pairs_d) {
	unpacker_test(_d);
}
static struct cpondir unpacker_map_d[] = {
	{(struct rpcdir){
		 .name = "get",
		 .param = "Int",
		 .result = "String",
		 .flags = RPCDIR_F_GETTER,
		 .access = RPCACCESS_READ,
		 .signals =
			 (const struct rpcdirsig[]){
				 {.name = "chng"},
				 {.name = "maint", .param = "Bool"},
			 },
		 .signals_cnt = 2,
	 },
		"{\"name\":\"get\",\"flags\":2,\"param\":\"Int\",\"result\":\"String\",\"access\":8,\"signals\":{\"chng\":null,\"maint\":\"Bool\"}}"},
	{(struct rpcdir){.name = "name", .access = RPCACCESS_READ},
		"{\"name\":\"name\",\"accessGrant\":\"rd\",\"unknown\":0}"},
	{(struct rpcdir){.name = "name", .access = RPCACCESS_READ},
		"i{1:\"name\",5:8,63:{}}"},
	{(struct rpcdir){.name = "name", .access = RPCACCESS_READ},
		"i{1:\"name\",5:8,6:{}}"},
};
ARRAY_TEST(unpack, unpacker_map) {
	unpacker_test(_d);
}

static const char *const unpack_invalid_d[] = {
	"i{}",
	"{}",
	"[]",
	"i{1:\"ok\"}",
	"i{1:\"ok\",5:\"invalid\"}",
	"i{1:\"ok\",5:false}",
	"i{2:\"well\"}",
	"i{1:null}",
	"i{1:\"ok\",2:\"foo\"}",
	"i{1:\"ok\",3:42}",
	"i{1:\"ok\",4:42}",
	"i{1:\"ok\",6:[]}",
	"{\"accessGrant\":\"invalid\"}",
};
ARRAY_TEST(unpack, unpack_invalid) {
	struct cpitem item;
	cpitem_unpack_init(&item);
	cp_unpack_t unpack = unpack_cpon(_d);
	struct obstack obstack;
	obstack_init(&obstack);
	void *obase = obstack_base(&obstack);
	ck_assert_ptr_null(rpcdir_unpack(unpack, &item, &obstack));
	ck_assert_ptr_eq(obstack_base(&obstack), obase);
	obstack_free(&obstack, NULL);
	unpack_free(unpack);
}
