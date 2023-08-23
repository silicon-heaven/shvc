#ifndef SHV_RPCHANDLER_H
#define SHV_RPCHANDLER_H

#include "shv/rpcmsg.h"
#include <stdbool.h>
#include <shv/rpcclient.h>


#define RPCH_F_KEEP_META

struct rpchandler {
	int flags;
	void *(*init)(rpcclient_t);
	void (*deinit)(void *cookie, rpcclient_t);
	bool (*handle_request)(void *cookie, rpcclient_t, const char *path,
		const char *method, const struct rpcmsg_request_info *info);
	bool (*handle_response)(void *cookie, rpcclient_t, int64_t rid, bool error);
	bool (*handle_signal)(
		void *cookie, rpcclient_t, const char *path, const char *method);
};


bool rpchandler_loop(rpcclient_t, struct rpchandler *);

typedef struct rpchandler_state *rpchandler_state_t;

rpchandler_state_t rpchandler_new(rpcclient_t, struct rpchandler *);

bool rpchandler_next(rpchandler_state_t);

void rpchandler_destroy(rpchandler_state_t);



#endif
