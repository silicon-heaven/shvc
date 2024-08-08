#include "broker.h"

static int subcmp(const void *a, const void *b) {
	const struct subscription *da = a;
	const struct subscription *db = b;
	return strcmp(da->ri, db->ri);
}

const char *subscription_ri(struct rpcbroker *broker, const char *ri) {
	struct subscription ref = {.ri = ri};
	struct subscription *sub = ARR_BSEARCH(&ref, broker->subscriptions, subcmp);
	return sub ? sub->ri : NULL;
}

bool subscribe(struct rpcbroker *broker, const char *ri, int cid) {
	struct subscription ref = {.ri = ri};
	struct subscription *sub = ARR_BSEARCH(&ref, broker->subscriptions, subcmp);
	if (sub) {
		if (nbool(sub->clients, cid))
			return false;
		nbool_set(&sub->clients, cid);
		return true;
	}

	sub = ARR_ADD(broker->subscriptions);
	sub->ri = strdup(ri);
	sub->clients = NULL;
	nbool_set(&sub->clients, cid);
	ARR_QSORT(broker->subscriptions, subcmp);
	return true;
}

bool unsubscribe(struct rpcbroker *broker, const char *ri, int cid) {
	struct subscription ref = {.ri = ri};
	struct subscription *sub = ARR_BSEARCH(&ref, broker->subscriptions, subcmp);
	if (!sub || !nbool(sub->clients, cid))
		return false;
	nbool_clear(&sub->clients, cid);
	if (!sub->clients) {
		free((char *)sub->ri);
		ARR_DEL(broker->subscriptions, sub);
	}
	return true;
}

void unsubscribe_all(struct rpcbroker *broker, int cid) {
	size_t i = 0;
	while (i < broker->subscriptions_cnt) {
		nbool_clear(&broker->subscriptions[i].clients, cid);
		if (broker->subscriptions[i].clients == NULL) {
			free((char *)broker->subscriptions[i].ri);
			ARR_DEL(broker->subscriptions, broker->subscriptions + i);
		} else
			i++;
	}
}
