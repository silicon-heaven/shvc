#ifndef SHV_RPCPING_H
#define SHV_RPCPING_H
#include <shv/rpcmsg.h>

static inline size_t rpcping_pack(cp_pack_t pack, int rid) {
	return rpcmsg_pack_request_void(pack, "broker/app", "ping", rid);
}

#endif
