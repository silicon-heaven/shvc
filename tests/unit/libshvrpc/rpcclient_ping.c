#include "rpcclient_ping.h"
#include <stdlib.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <shv/rpcmsg.h>
#include <check.h>

void rpcclient_ping_test(rpcclient_t c) {
	rpcmsg_pack_request_void(rpcclient_pack(c), ".broker/app", "ping", 3);
	ck_assert(rpcclient_sendmsg(c));

	struct obstack obs;
	obstack_init(&obs);
	struct cpitem item = (struct cpitem){};
	struct rpcmsg_meta meta;
	ck_assert(rpcclient_nextmsg(c));
	ck_assert(rpcmsg_head_unpack(rpcclient_unpack(c), &item, &meta, NULL, &obs));
	ck_assert_int_eq(meta.type, RPCMSG_T_RESPONSE);
	ck_assert_int_eq(meta.request_id, 3);
	ck_assert(rpcclient_validmsg(c));
	obstack_free(&obs, NULL);
}
