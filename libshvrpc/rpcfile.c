#include <stdlib.h>
#include <shv/rpcfile.h>

struct rpcdir rpcfile_stat = {
	.name = "stat",
	.result = "!stat",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_READ,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_size = {
	.name = "size",
	.result = "i(0,)",
	.flags = RPCDIR_F_GETTER,
	.access = RPCACCESS_READ,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_crc = {
	.name = "crc",
	.param = "[i(0,):offset,i(0,)|n:size]|n",
	.result = "u(>32)",
	.flags = 0,
	.access = RPCACCESS_READ,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_sha1 = {
	.name = "sha1",
	.param = "[i(0,):offset,i(0,)|n:size]|n",
	.result = "b(20)",
	.flags = 0,
	.access = RPCACCESS_READ,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_read = {
	.name = "read",
	.param = "[i(0,):offset,i(0,)size]",
	.result = "b",
	.flags = RPCDIR_F_LARGE_RESULT,
	.access = RPCACCESS_READ,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_write = {
	.name = "write",
	.param = "[i(0,):offset,b:data]",
	.flags = 0,
	.access = RPCACCESS_WRITE,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_truncate = {
	.name = "truncate",
	.param = "i(0,)",
	.flags = 0,
	.access = RPCACCESS_WRITE,
	.signals_cnt = 0,
};

struct rpcdir rpcfile_append = {
	.name = "append",
	.param = "b",
	.flags = 0,
	.access = RPCACCESS_WRITE,
	.signals_cnt = 0,
};

bool rpcfile_stat_pack(cp_pack_t pack, struct rpcfile_stat_s *stats) {
	cp_pack_imap_begin(pack);

	cp_pack_int(pack, RPCFILE_STAT_KEY_TYPE);
	cp_pack_int(pack, stats->type);

	cp_pack_int(pack, RPCFILE_STAT_KEY_SIZE);
	cp_pack_int(pack, stats->size);

	cp_pack_int(pack, RPCFILE_STAT_KEY_PAGESIZE);
	cp_pack_int(pack, stats->page_size);

	cp_pack_int(pack, RPCFILE_STAT_KEY_ACCESSTIME);
	cp_pack_datetime(pack, stats->access_time);

	cp_pack_int(pack, RPCFILE_STAT_KEY_MODTIME);
	cp_pack_datetime(pack, stats->mod_time);

	cp_pack_int(pack, RPCFILE_STAT_KEY_MAXWRITE);
	cp_pack_int(pack, stats->max_write);

	if (stats->max_read != 0) {
		cp_pack_int(pack, RPCFILE_STAT_KEY_MAXREAD);
		cp_pack_int(pack, stats->max_read);
	}

	if (stats->erase_size != 0) {
		cp_pack_int(pack, RPCFILE_STAT_KEY_ERASESIZE);
		cp_pack_int(pack, stats->erase_size);
	}

	cp_pack_container_end(pack);
	return true;
}

struct rpcfile_stat_s *rpcfile_stat_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	struct rpcfile_stat_s *res = obstack_alloc(obstack, sizeof(*res));
	*res = (struct rpcfile_stat_s){
		.type = 0,
		.size = 0,
		.page_size = 0,
		.access_time = {.msecs = 0, .offutc = 0},
		.mod_time = {.msecs = 0, .offutc = 0},
	};

	if (cp_unpack_type(unpack, item) != CPITEM_IMAP) {
		obstack_free(obstack, res);
		return NULL;
	}

	for_cp_unpack_imap(unpack, item, ikey) {
		switch (ikey) {
			case RPCFILE_STAT_KEY_TYPE:
				cp_unpack_int(unpack, item, res->type);
				break;
			case RPCFILE_STAT_KEY_SIZE:
				cp_unpack_int(unpack, item, res->size);
				break;
			case RPCFILE_STAT_KEY_PAGESIZE:
				cp_unpack_int(unpack, item, res->page_size);
				break;
			case RPCFILE_STAT_KEY_ACCESSTIME:
				cp_unpack_datetime(unpack, item, res->access_time);
				break;
			case RPCFILE_STAT_KEY_MODTIME:
				cp_unpack_datetime(unpack, item, res->mod_time);
				break;
			case RPCFILE_STAT_KEY_MAXWRITE:
				cp_unpack_int(unpack, item, res->max_write);
				break;
			case RPCFILE_STAT_KEY_MAXREAD:
				cp_unpack_int(unpack, item, res->max_read);
				break;
			case RPCFILE_STAT_KEY_ERASESIZE:
				cp_unpack_int(unpack, item, res->erase_size);
				break;
			default:
				cp_unpack_skip(unpack, item);
				break;
		}
	}

	return res;
}
