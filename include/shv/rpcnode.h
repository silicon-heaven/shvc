#ifndef SHV_RPCNODE_H
#define SHV_RPCNODE_H

#include <obstack.h>
#include <shv/rpcmsg.h>

#define RPCNODE_DIR_

struct rpcnode_dir_request {
	const char *name;
	bool list_of_maps;
};

enum rpcnode_dir_signature {
	RPCNODE_DIR_VOID_VOID,
	RPCNODE_DIR_VOID_PARAM,
	RPCNODE_DIR_RET_VOID,
	RPCNODE_DIR_RET_PARAM,
};

#define RPCNODE_DIR_F_SIGNAL (1 << 0)
#define RPCNODE_DIR_F_GETTER (1 << 1)
#define RPCNODE_DIR_F_SETTER (1 << 2)
#define RPCNODE_DIR_F_LARGE_RESULT_HINT (1 << 3)

bool rpcnode_handle_dir(cp_unpack_t unpack, struct cpitem *item,
	struct rpcnode_dir_request *dirr, struct obstack *obstack)
	__attribute__((nonnull));

bool rpcnode_pack_dir(cp_pack_t pack, bool list_of_maps, const char *name,
	enum rpcnode_dir_signature signature, int flags, enum rpcmsg_access acc,
	const char *description) __attribute__((nonnull(1, 3)));


typedef int rpcnode_ls_request;

rpcnode_ls_request rpcnode_unpack_ls(cp_unpack_t unpack, struct cpitem *item);

void rpcnode_pack_ls(cp_pack_t pack);


#endif
