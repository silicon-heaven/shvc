#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_login.h>
#include <shv/rpcurl.h>
#include "handler.h"
#include "opts.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

static volatile sig_atomic_t halt = false;

static void sigint_handler(int status) {
	halt = true;
}

int main(int argc, char **argv) {
	struct conf conf;
	parse_opts(argc, argv, &conf);

	struct obstack obstack;
	obstack_init(&obstack);
	const char *errpos;
	struct rpcurl *rpcurl = rpcurl_parse(conf.url, &errpos, &obstack);
	if (rpcurl == NULL) {
		fprintf(stderr, "Invalid URL: %s\n", conf.url);
		fprintf(stderr, "%*.s^\n", 13 + (int)(errpos - conf.url), "");
		return 1;
	}
	rpcclient_t client = rpcurl_connect_client(rpcurl);
	if (client == NULL) {
		fprintf(stderr, "Failed to connect to the: %s\n", rpcurl->location);
		fprintf(stderr, "Please check your connection to the network\n");
		obstack_free(&obstack, NULL);
		return 1;
	}
	client->logger_in =
		rpclogger_new(rpclogger_func_stderr, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(rpclogger_func_stderr, "=> ", logsiz, conf.verbose);

	struct device_state *state = device_state_new();

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_app_t app = rpchandler_app_new("shvc-demo-device", PROJECT_VERSION);

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
	obstack_free(&obstack, NULL);
	return 0;
}
