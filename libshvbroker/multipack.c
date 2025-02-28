#include "multipack.h"
#include <shv/rpcri.h>


nbool_t signal_destinations(struct rpcbroker *broker, const char *path,
	const char *source, const char *signal, rpcaccess_t access) {
	nbool_t res = NULL;
	for (size_t i = 0; i < broker->subscriptions_cnt; i++)
		if (rpcri_match(broker->subscriptions[i].ri, path, source, signal))
			nbool_or(&res, broker->subscriptions[i].clients);
	for_nbool(res, cid) if (!cid_active(broker, cid) ||
		broker->clients[cid]->role->access(
			broker->clients[cid]->role->access_cookie, path, source) < access)
		nbool_clear(&res, cid);
	return res; // NOLINT(clang-analyzer-unix.Malloc)
}


static bool pack_func(void *ptr, const struct cpitem *item) {
	struct multipack *p = ptr;
	bool res = false;
	for (size_t i = 0; i < p->cnt; i++)
		if (p->packs[i]) {
			if (cp_pack(p->packs[i], item))
				res = true;
			else
				p->packs[i] = NULL;
		}
	return res;
}

cp_pack_t multipack_init(struct rpcbroker *broker, struct multipack *multipack,
	nbool_t destinations) {
	cp_pack_t *packs = malloc(nbool_nbits(destinations) * sizeof *packs);
	size_t cnt = 0;
	for_nbool(destinations, cid) {
		packs[cnt++] = rpchandler_msg_new(broker->clients[cid]->handler);
	}
	*multipack = (struct multipack){pack_func, packs, cnt};
	return &multipack->func;
}

void multipack_done(struct rpcbroker *broker, struct multipack *multipack,
	nbool_t destinations, bool send) {
	size_t i = 0;
	for_nbool(destinations, cid) {
		if (multipack->packs[i++] && send)
			rpchandler_msg_send(broker->clients[cid]->handler);
		else
			rpchandler_msg_drop(broker->clients[cid]->handler);
	}
	free(multipack->packs);
}
