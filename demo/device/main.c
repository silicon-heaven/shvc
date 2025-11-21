#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <shv/rpcclient.h>
#include <shv/rpcfile.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_file.h>
#include <shv/rpchandler_login.h>
#include <shv/rpcurl.h>
#include "handler.h"
#include "opts.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#define DEMO_FILE_PATH "demo_device_shv_file"

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

static volatile sig_atomic_t halt = false;

static struct rpchandler_file_list file_corr[] = {
	{
		"file",
		DEMO_FILE_PATH,
		RPCFILE_ACCESS_VALIDATION | RPCFILE_ACCESS_READ | RPCFILE_ACCESS_WRITE |
			RPCFILE_ACCESS_TRUNCATE | RPCFILE_ACCESS_APPEND,
	},
};

static void sigint_handler(int status) {
	halt = true;
}

static struct rpchandler_file_list *list_files(int *num) {
	*num = sizeof(file_corr) / sizeof(struct rpchandler_file_list);
	struct rpchandler_file_list *corr =
		calloc(*num, sizeof(struct rpchandler_file_list));
	memcpy(corr, file_corr, *num * sizeof(struct rpchandler_file_list));
	return corr;
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
		rpclogger_new(&rpclogger_stderr_funcs, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(&rpclogger_stderr_funcs, "=> ", logsiz, conf.verbose);

	struct device_state *state = device_state_new();

	int fd = open(DEMO_FILE_PATH, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	close(fd);

	struct rpchandler_file_cb file_cb = {
		.ls = list_files,
		.update_paths = NULL,
	};

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_file_t file = rpchandler_file_new(&file_cb);

	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(&(struct rpchandler_app_conf){
			.name = "shvc-demo-device", .version = PROJECT_VERSION}),
		rpchandler_file_stage(file),
		(struct rpchandler_stage){.funcs = &device_handler_funcs, .cookie = state},
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);
	signal(SIGINT, sigint_handler);
	signal(SIGHUP, sigint_handler);
	signal(SIGTERM, sigint_handler);

	rpchandler_run(handler, &halt);

	int ec = 1;
	if (rpcclient_errno(client)) {
		fprintf(stderr, "Connection failure: %s\n",
			strerror(rpcclient_errno(client)));
	} else {
		rpcerrno_t login_errnum;
		const char *login_errmsg;
		rpchandler_login_status(login, &login_errnum, &login_errmsg);
		if (login_errnum) {
			fprintf(stderr, "Login failure: %s\n",
				login_errmsg ?: rpcerror_str(login_errnum));
		} else
			ec = 0;
	}

	rpchandler_destroy(handler);
	rpchandler_file_destroy(file);
	rpchandler_login_destroy(login);
	device_state_free(state);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	obstack_free(&obstack, NULL);
	return ec;
}
