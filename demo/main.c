#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpchandler_app.h>
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
	rpclogger_t logger = NULL;
	if (conf.verbose > 0) {
		logger = rpclogger_new(stderr, conf.verbose);
		client->logger = logger;
	}
	char *loginerr;
	if (!rpcclient_login(client, &rpcurl->login, &loginerr)) {
		if (loginerr) {
			fprintf(stderr, "Invalid login for connecting to the: %s\n", conf.url);
			fprintf(stderr, "%s\n", loginerr);
		} else {
			fprintf(stderr, "Communication error with server\n");
		}
		rpcclient_destroy(client);
		rpcurl_free(rpcurl);
		return 1;
	}
	rpcurl_free(rpcurl);

	struct device_state *state = device_state_new();

	rpchandler_app_t app = rpchandler_app_new("demo-device", PROJECT_VERSION);

	const struct rpchandler_stage stages[] = {
		rpchandler_app_stage(app),
		(struct rpchandler_stage){.funcs = &device_handler_funcs, .cookie = state},
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);

	rpchandler_run(handler, NULL);
	printf("Terminating due to: %s\n", strerror(rpcclient_errno(client)));

	rpchandler_destroy(handler);
	rpchandler_app_destroy(app);
	device_state_free(state);
	rpcclient_destroy(client);
	if (conf.verbose > 0)
		rpclogger_destroy(logger);
	return 0;
}
