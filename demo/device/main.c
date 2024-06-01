#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <poll.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_login.h>
#include "opts.h"
#include "handler.h"

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

static volatile sig_atomic_t halt = false;

static void sigint_handler(int status) {
	halt = true;
}

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
	client->logger_in =
		rpclogger_new(rpclogger_func_stderr, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(rpclogger_func_stderr, "=> ", logsiz, conf.verbose);

	struct device_state *state = device_state_new();

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_app_t app = rpchandler_app_new("demo-device", PROJECT_VERSION);

	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(app),
		(struct rpchandler_stage){.funcs = &device_handler_funcs, .cookie = state},
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);
	signal(SIGINT, sigint_handler);
	signal(SIGHUP, sigint_handler);
	signal(SIGTERM, sigint_handler);

	// TODO terminate on failed login

	rpchandler_run(handler, &halt);
	printf("Terminating due to: %s\n", strerror(rpcclient_errno(client)));

	rpchandler_destroy(handler);
	rpchandler_app_destroy(app);
	rpchandler_login_destroy(login);
	device_state_free(state);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	rpcurl_free(rpcurl);
	return 0;
}
