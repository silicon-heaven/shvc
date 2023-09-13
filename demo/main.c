#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpctoplevel.h>
#include "opts.h"
#include "device_handler.h"


int main(int argc, char **argv) {
	struct conf conf;
	parse_opts(argc, argv, &conf);

	const char *errpos;
	struct rpcurl *rpcurl = rpcurl_parse(conf.url, &errpos);
	if (rpcurl == NULL) {
		fprintf(stderr, "Invalid URL: %s\n", conf.url);
		fprintf(stderr, "%*.s^\n", 13 + (int)(errpos - conf.url), "");
		return 1;
	}
	rpcclient_t client = rpcclient_connect(rpcurl);
	if (client == NULL) {
		fprintf(stderr, "Failed to connect to the: %s\n", rpcurl->location);
		fprintf(stderr, "Please check your connection to the network\n");
		rpcurl_free(rpcurl);
		return 1;
	}
	rpcclient_logger_t logger = NULL;
	if (conf.verbose > 0) {
		logger = rpcclient_logger_new(stderr, conf.verbose);
		client->logger = logger;
	}
	switch (rpcclient_login(client, &rpcurl->login)) {
		case RPCCLIENT_LOGIN_OK:
			break;
		case RPCCLIENT_LOGIN_INVALID:
			fprintf(stderr, "Invalid login for connecting to the: %s\n", conf.url);
			rpcclient_destroy(client);
			rpcurl_free(rpcurl);
			return 1;
		case RPCCLIENT_LOGIN_ERROR:
			fprintf(stderr, "Communication error with server\n");
			return 2;
	}
	rpcurl_free(rpcurl);

	rpctoplevel_t toplevel = rpctoplevel_new("demo-device", PROJECT_VERSION, true);

	struct demo_handler h = {.func = demo_device_handler_func};
	const rpchandler_func *funcs[] = {
		rpctoplevel_func(toplevel),
		&h.func,
		NULL,
	};
	rpchandler_t handler = rpchandler_new(client, funcs, NULL);
	assert(handler);

	int res = 0;
	struct pollfd pfd = {.fd = client->rfd, .events = POLLIN | POLLHUP};
	while (true) {
		int pr = poll(
			&pfd, 1, rpcclient_maxsleep(client, RPC_DEFAULT_IDLE_TIME) * 1000);
		if (pr == 0) {
			cp_pack_t pack = rpchandler_msg_new(handler);
			rpcmsg_pack_request_void(pack, ".broker/app", "ping", 0);
			rpchandler_msg_send(handler);
		} else if (pr == -1 || pfd.revents & POLLERR) {
			fprintf(stderr, "Poll error: %s\n", strerror(errno));
			res = 1;
			break;
		} else if (pfd.revents & POLLIN) {
			rpchandler_next(handler);
		} else if (pfd.revents & POLLHUP) {
			fprintf(stderr, "Disconnected from the broker\n");
			break;
		}
	}

	rpchandler_destroy(handler);
	rpctoplevel_destroy(toplevel);
	rpcclient_destroy(client);
	rpcclient_logger_destroy(logger);
	return res;
}
