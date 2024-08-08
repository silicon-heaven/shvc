#ifndef SHVBROKER_BROKER_H
#define SHVBROKER_BROKER_H

#include <stdlib.h>
#include <pthread.h>
#include <shv/rpcbroker.h>

#include "arr.h"
#include "nbool.h"
#include "util.h"

#define IN_USE ((time_t) - 1)
#define REUSE_TIMEOUT (600) /* Ten minutes before client ID reuse */
#define NONCE_LEN (10)
#define IDLE_TIMEOUT_LOGIN (5)

struct rpcbroker {
	const char *name;
	rpcbroker_login_t login;
	void *login_cookie;

	struct clientctx {
		/* Last use of this client context. It marks this slot for reuse after
		 * REUSE_TIMEOUT. When slot is in active use it is set to `IN_USE`.
		 */
		time_t lastuse;
		/* RPC handler of this client */
		rpchandler_t handler;
		/* Login */
		char nonce[NONCE_LEN + 1];
		char *username;
		unsigned activity_timeout;
		time_t last_activity;
		/* Role of the client in the broker. */
		const struct rpcbroker_role *role;
		struct rpcbroker *broker;
		/* Subscriptions TTL */
		ARR(
			struct ttlsub {
				const char *ri;
				time_t ttl;
			},
			ttlsubs);
	} *clients;
	size_t clients_cnt;

	ARR(
		struct mount {
			const char *path;
			int cid;
		},
		mounts);

	ARR(
		struct subscription {
			const char *ri;
			nbool_t clients;
		},
		subscriptions);

	pthread_mutex_t lock;
	bool use_lock;
};

__attribute__((nonnull)) static inline void broker_lock(struct rpcbroker *broker) {
	// TODO possibly could be RW lock
	if (broker->use_lock)
		pthread_mutex_lock(&broker->lock);
}

__attribute__((nonnull)) static inline void broker_unlock(struct rpcbroker *broker) {
	if (broker->use_lock)
		pthread_mutex_unlock(&broker->lock);
}

/* Iterate over all client IDs.
 *
 * You should combine this with `cid_valid` to get only valid clients.
 *
 * Make sure to call this while holding lock.
 */
#define for_cid(BROKER) for (int cid = 0; cid < (BROKER)->clients_cnt; cid++)

/* Check if given CID is valid for some client of this broker.
 *
 * Make sure to call this while holding lock.
 */
__attribute__((nonnull)) static inline bool cid_valid(
	struct rpcbroker *broker, int cid) {
	return cid >= 0 && cid < broker->clients_cnt &&
		broker->clients[cid].lastuse == IN_USE;
}

/* Check if given CID is valid and has assigned role for some client of this
 * broker.
 *
 * Make sure to call this while holding lock.
 */
__attribute__((nonnull)) static inline bool cid_active(
	struct rpcbroker *broker, int cid) {
	return cid_valid(broker, cid) && broker->clients[cid].role;
}

/* Allocate a new CID.
 *
 * Make sure to call this while holding lock.
 */
__attribute__((nonnull)) static inline int cid_new(struct rpcbroker *broker) {
	time_t now = get_time();
	int cid;
	for (cid = 0; cid < broker->clients_cnt &&
		 (broker->clients[cid].lastuse >= now - REUSE_TIMEOUT ||
			 broker->clients[cid].lastuse == IN_USE);
		 cid++) {}
	if (cid == broker->clients_cnt) {
		broker->clients_cnt *= 2;
		struct clientctx *nclients = realloc(
			broker->clients, broker->clients_cnt * sizeof *broker->clients);
		assert(nclients); // TODO
		broker->clients = nclients;
		memset(broker->clients + cid, 0,
			(broker->clients_cnt / 2) * sizeof *broker->clients);
	}
	return cid;
}

/* Free the CID.
 *
 * Make sure to call this while holding lock.
 */
__attribute__((nonnull)) static inline void cid_free(
	struct rpcbroker *broker, int cid) {
	broker->clients[cid].handler = NULL;
	broker->clients[cid].lastuse = get_time();
}

enum role_res {
	ROLE_RES_OK,
	ROLE_RES_MNT_INVALID,
	ROLE_RES_MNT_EXISTS,
} role_assign(struct clientctx *clientctx, const struct rpcbroker_role *role)
	__attribute__((nonnull));

void role_unassign(struct clientctx *c) __attribute__((nonnull));

struct clientctx *mounted_client(struct rpcbroker *broker, const char *path,
	const char **rpath) __attribute__((nonnull(1, 2)));

static inline bool is_path_prefix(const char *path, const char *prefix) {
	size_t len = strlen(prefix);
	if (len == 0)
		return true; /* All paths are relative to the root (empty prefix) */
	return !strncmp(path, prefix, len) && (path[len] == '/' || path[len] == '\0');
}

const char *subscription_ri(struct rpcbroker *broker, const char *ri)
	__attribute__((nonnull));

bool subscribe(struct rpcbroker *broker, const char *ri, int cid)
	__attribute__((nonnull));

bool unsubscribe(struct rpcbroker *broker, const char *ri, int cid)
	__attribute__((nonnull));

void unsubscribe_all(struct rpcbroker *broker, int cid) __attribute__((nonnull));

#endif
