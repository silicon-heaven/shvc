#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <shv/rpcclient.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_history.h>
#include <shv/rpchandler_login.h>
#include <shv/rpcurl.h>
#include "opts.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

static const struct rpchistory_record_head heads[] = {
	(struct rpchistory_record_head){
		.type = RPCHISTORY_RECORD_NORMAL,
		.datetime = {.msecs = 150000, .offutc = 0},
	},
	(struct rpchistory_record_head){
		.type = RPCHISTORY_RECORD_NORMAL,
		.access = RPCACCESS_BROWSE,
		.path = "get2",
		.signal = "fchng",
		.source = "src",
		.userid = "elluser",
		.datetime = {.msecs = 500000, .offutc = 0},
	},
	(struct rpchistory_record_head){
		.type = RPCHISTORY_RECORD_TIMEJUMP,
		.access = RPCACCESS_READ,
		.timejump = 18000000,
		.datetime = {.msecs = 500000, .offutc = 0},
	},
	(struct rpchistory_record_head){
		.type = RPCHISTORY_RECORD_NORMAL,
		.access = RPCACCESS_WRITE,
		.path = "set3",
		.userid = "elluser_wifi",
		.datetime = {.msecs = 600000, .offutc = 0},
	},
	(struct rpchistory_record_head){
		.type = RPCHISTORY_RECORD_NORMAL,
		.access = RPCACCESS_SERVICE,
		.path = "get6",
		.userid = "elluser_local",
		.datetime = {.msecs = 800000, .offutc = 0},
	},
};

static volatile sig_atomic_t halt = false;

static void sigint_handler(int status) {
	halt = true;
}

static bool log_pack_record(
	void *log, int index, cp_pack_t pack, struct obstack *obstack) {
	if (index < 2 || index > 6)
		return false;

	struct rpchistory_record_head head;
	memcpy(&head, &heads[index - 2], sizeof(struct rpchistory_record_head));
	rpchistory_record_pack_begin(pack, &head);
	if (head.type == RPCHISTORY_RECORD_NORMAL) {
		cp_pack_int(pack, index);
	}
	rpchistory_record_pack_end(pack);

	return true;
}

static bool log_get_index_range(
	void *log, uint64_t *min, uint64_t *max, uint64_t *span) {
	*min = 1;
	*max = 6;
	*span = 6;
	return true;
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

	struct rpchandler_history_records *records[2] = {
		&(struct rpchandler_history_records){.name = "records_log",
			.cookie = NULL,
			.get_index_range = log_get_index_range,
			.pack_record = log_pack_record},
		NULL};

	struct rpchandler_history_facilities facilities = {
		.records = records, .files = NULL};

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_app_t app = rpchandler_app_new("shvc-demo-history", PROJECT_VERSION);
	rpchandler_history_t history = rpchandler_history_new(&facilities);

	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(app),
		rpchandler_history_stage(history),
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
	rpchandler_history_destroy(history);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	obstack_free(&obstack, NULL);
	return 0;
}
