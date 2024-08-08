#include <stdlib.h>
#include <limits.h>
#include <sys/epoll.h>
#include <shv/rpcbroker.h>

enum evtype {
	EVT_SERVER,
	EVT_PEER,
};

struct evgeneric {
	enum evtype type;
};

struct evserver {
	enum evtype type;
	rpcserver_t server;
};

struct evpeer {
	enum evtype type;
	int cid;
	struct evpeer *prev, *next;
};


__attribute__((nonnull)) static inline void evpeer_add(
	const struct rpcbroker_state *state, int epfd, rpcserver_t server,
	struct evpeer **last) {
	rpcclient_t client = rpcserver_accept(server);
	if (!client)
		return;
	int cid = state->new_client(state->cookie, state->broker, server, client);
	if (cid < 0)
		return;
	struct evpeer *ev = malloc(sizeof *ev);
	*ev = (struct evpeer){
		.type = EVT_PEER,
		.cid = cid,
		.next = NULL,
		.prev = *last,
	};
	if (*last)
		(*last)->next = ev;
	*last = ev;

	struct epoll_event eev;
	eev.events = EPOLLIN | EPOLLHUP;
	eev.data.ptr = ev;
	epoll_ctl( // TODO epoll error?
		epfd, EPOLL_CTL_ADD, rpcclient_pollfd(client), &eev);
}

__attribute__((nonnull)) static inline struct evpeer *evpeer_del(
	const struct rpcbroker_state *state, int epfd, struct evpeer *ev,
	struct evpeer **last) {
	epoll_ctl(epfd, EPOLL_CTL_DEL,
		rpcclient_pollfd(
			rpchandler_client(rpcbroker_client_handler(state->broker, ev->cid))),
		NULL);
	state->del_client(state->cookie, state->broker, ev->cid);
	if (ev->prev)
		ev->prev->next = ev->next;
	if (ev->next)
		ev->next->prev = ev->prev;
	else
		*last = ev->prev;
	struct evpeer *res = ev->prev;
	free(ev);
	return res;
}

void rpcbroker_run(
	const struct rpcbroker_state *state, volatile sig_atomic_t *halt) {
	int epfd = epoll_create1(0);

	struct evserver evservers[state->servers_cnt];
	for (size_t i = 0; i < state->servers_cnt; i++) {
		evservers[i] = (struct evserver){
			.type = EVT_SERVER,
			.server = state->servers[i],
		};
		struct epoll_event eev;
		eev.events = EPOLLIN | EPOLLHUP;
		eev.data.ptr = &evservers[i];
		epoll_ctl( // TODO epoll error?
			epfd, EPOLL_CTL_ADD, rpcserver_pollfd(state->servers[i]), &eev);
	}

	struct evpeer *latest_peer = NULL;

	int timeout = 0;
	while (!halt || !*halt) {
		struct epoll_event eev;
		int pr = epoll_wait(epfd, &eev, 1, timeout);
		if (pr == -1) {
			if (errno != EINTR)
				abort(); // TODO
			timeout = 0;
		} else if (pr != 0) {
			timeout = 0;
			struct evgeneric *evg = eev.data.ptr;
			switch (evg->type) {
				case EVT_SERVER:
					struct evserver *evs = eev.data.ptr;
					evpeer_add(state, epfd, evs->server, &latest_peer);
					break;
				case EVT_PEER:
					struct evpeer *evp = eev.data.ptr;
					rpchandler_t h =
						rpcbroker_client_handler(state->broker, evp->cid);
					if (h && !rpchandler_next(h))
						evpeer_del(state, epfd, evp, &latest_peer);
					break;
			}
		} else {
			timeout = INT_MAX;
			for (struct evpeer *ev = latest_peer; ev;) {
				int ntimeout = rpchandler_idling(
					rpcbroker_client_handler(state->broker, ev->cid));
				if (ntimeout < 0) {
					ev = evpeer_del(state, epfd, ev, &latest_peer);
					continue;
				} else if (ntimeout < timeout)
					timeout = ntimeout;
				ev = ev->prev;
			}
		}
	}

	/* The connection to all clients is first terminated before we unregister
	 * them to ensure that we do not attempt to send signals.
	 */
	for (struct evpeer *ev = latest_peer; ev; ev = ev->prev)
		rpcclient_disconnect(
			rpchandler_client(rpcbroker_client_handler(state->broker, ev->cid)));
	while (latest_peer)
		evpeer_del(state, epfd, latest_peer, &latest_peer);
	close(epfd);
}
