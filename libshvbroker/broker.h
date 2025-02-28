#ifndef SHVBROKER_BROKER_H
#define SHVBROKER_BROKER_H

#include <stdlib.h>
#include <pthread.h>
#include <shv/rpcbroker.h>

#include "arr.h"
#include "nbool.h"

#define REUSE_TIMEOUT (600) /* Ten minutes before client ID reuse */
#define NONCE_LEN (10)
#define IDLE_TIMEOUT_LOGIN (5)

struct clientctx {
	int cid;
	struct rpcbroker *broker;
	/* RPC handler of this client */
	rpchandler_t handler;
	/* Role of the client in the broker. */
	const struct rpcbroker_role *role;
	/* Login */
	bool login;
	char nonce[NONCE_LEN + 1];
	char *username;
	unsigned activity_timeout;
	time_t last_activity;
	/* Subscriptions TTL */
	ARR(
		struct ttlsub {
			const char *ri;
			time_t ttl;
		},
		ttlsubs);
};

struct rpcbroker {
	const char *name;
	int flags;
	rpcbroker_login_t login;
	void *login_cookie;

	struct clientctx **clients;
	time_t *clients_lastuse;
	size_t clients_siz;

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
};

[[gnu::nonnull]]
static inline void broker_lock(struct rpcbroker *broker) {
	// TODO possibly could be RW lock
	if (!(broker->flags & RPCBROKER_F_NOLOCK))
		pthread_mutex_lock(&broker->lock);
}

[[gnu::nonnull]]
static inline void broker_unlock(struct rpcbroker *broker) {
	if (!(broker->flags & RPCBROKER_F_NOLOCK))
		pthread_mutex_unlock(&broker->lock);
}

/* Iterate over all client IDs.
 *
 * You should combine this with `cid_valid` to get only valid clients.
 *
 * Make sure to call this while holding lock.
 */
#define for_cid(BROKER) for (int cid = 0; cid < (BROKER)->clients_siz; cid++)

/* Check if given CID is valid for some client of this broker.
 *
 * Make sure to call this while holding lock.
 */
[[gnu::nonnull]]
static inline bool cid_valid(struct rpcbroker *broker, int cid) {
	return cid >= 0 && cid < broker->clients_siz && broker->clients[cid];
}

/* Check if given CID is valid and has assigned role for some client of this
 * broker.
 *
 * Make sure to call this while holding lock.
 */
[[gnu::nonnull]]
static inline bool cid_active(struct rpcbroker *broker, int cid) {
	return cid_valid(broker, cid) && broker->clients[cid]->role;
}

[[gnu::nonnull]]
enum role_res {
	ROLE_RES_OK,
	ROLE_RES_MNT_INVALID,
	ROLE_RES_MNT_EXISTS,
} role_assign(struct clientctx *clientctx, const struct rpcbroker_role *role);

[[gnu::nonnull]]
void role_unassign(struct clientctx *c);

[[gnu::nonnull(1, 2)]]
struct clientctx *mounted_client(
	struct rpcbroker *broker, const char *path, const char **rpath);

static inline bool is_path_prefix(const char *path, const char *prefix) {
	size_t len = strlen(prefix);
	if (len == 0)
		return true; /* All paths are relative to the root (empty prefix) */
	return !strncmp(path, prefix, len) && (path[len] == '/' || path[len] == '\0');
}

[[gnu::nonnull]]
const char *subscription_ri(struct rpcbroker *broker, const char *ri);

[[gnu::nonnull]]
bool subscribe(struct rpcbroker *broker, const char *ri, int cid);

[[gnu::nonnull]]
bool unsubscribe(struct rpcbroker *broker, const char *ri, int cid);

[[gnu::nonnull]]
void unsubscribe_all(struct rpcbroker *broker, int cid);

#endif
