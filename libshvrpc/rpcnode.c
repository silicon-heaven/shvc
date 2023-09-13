#include <shv/rpcnode.h>


bool rpcnode_handle_dir(cp_unpack_t unpack, struct cpitem *item,
	struct rpcnode_dir_request *dirr, struct obstack *obstack) {
	dirr->name = NULL;
	dirr->list_of_maps = true;
	if (item->type == CPITEM_CONTAINER_END)
		return true;

	bool list = false;
	cp_unpack(unpack, item);
	if (item->type == CPITEM_NULL)
		return true;
	if (item->type == CPITEM_LIST) {
		cp_unpack(unpack, item);
		list = true;
	}
	if (item->type == CPITEM_STRING) {
		dirr->name = cp_unpack_strdupo(unpack, item, obstack);
		cp_unpack(unpack, item);
		if (!list)
			return true;
	} else
		return item->type == CPITEM_CONTAINER_END;
	cp_unpack(unpack, item);
	if (item->type == CPITEM_INT) {
		dirr->list_of_maps = item->as.Int == 127;
		return true;
	} else if (item->type == CPITEM_CONTAINER_END)
		return true;
	return false;
}

bool rpcnode_pack_dir(cp_pack_t pack, bool list_of_maps, const char *name,
	enum rpcnode_dir_signature signature, int flags, enum rpcmsg_access acc,
	const char *description) {
	if (!list_of_maps)
		return cp_pack_str(pack, name);
	return cp_pack_map_begin(pack) && cp_pack_str(pack, "name") &&
		cp_pack_str(pack, name) && cp_pack_str(pack, "signature") &&
		cp_pack_int(pack, signature) && cp_pack_str(pack, "flags") &&
		cp_pack_int(pack, flags) && cp_pack_str(pack, "accessGrant") &&
		cp_pack_str(pack, rpcmsg_access_str(acc)) && cp_pack_container_end(pack);
}
