#include <stdio.h>
#include <stdlib.h>
#include <shv/cp_tools.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_impl.h>
#include <shv/rpchandler_login.h>
#include <shv/rpchandler_signals.h>
#include <shv/rpcurl.h>
#include "opts.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

static volatile sig_atomic_t halt = false;

static void sigint_handler(int status) {
	halt = true;
}

static void signalmsg(void *cookie, struct rpchandler_msg *msgctx) {
	char *buf = NULL;
	size_t bufsiz = 0;
	FILE *f = open_memstream(&buf, &bufsiz);
	fprintf(f, "<%s:%s:%s>", msgctx->meta.path, msgctx->meta.source,
		msgctx->meta.signal);
	if (rpcmsg_has_value(msgctx->item)) {
		struct cp_pack_cpon cp_pack_cpon;
		cp_pack_t cpon_pack = cp_pack_cpon_init(&cp_pack_cpon, f, "\t");
		cp_repack(msgctx->unpack, msgctx->item, cpon_pack);
		free(cp_pack_cpon.state.ctx);
	}
	fclose(f);
	if (rpchandler_msg_valid(msgctx)) {
		puts(buf);
		fflush(stdout);
	}
	free(buf);
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
		obstack_free(&obstack, NULL);
		return 3;
	}
	rpcclient_t client = rpcurl_connect_client(rpcurl);
	if (client == NULL) {
		fprintf(stderr, "Failed to connect to the: %s\n", conf.url);
		fprintf(stderr, "Please check your connection to the destination.\n");
		obstack_free(&obstack, NULL);
		return 3;
	}
	client->logger_in =
		rpclogger_new(&rpclogger_stderr_funcs, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(&rpclogger_stderr_funcs, "=> ", logsiz, conf.verbose);

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_signals_t signals = rpchandler_signals_new(signalmsg, NULL);
	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(&(struct rpchandler_app_conf){
			.name = "shvcsub", .version = PROJECT_VERSION}),
		rpchandler_signals_stage(signals),
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);
	signal(SIGINT, sigint_handler);
	signal(SIGHUP, sigint_handler);
	signal(SIGTERM, sigint_handler);

	for (char **ris = conf.ris; *ris; ris++)
		rpchandler_signals_subscribe(signals, *ris);

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
	rpchandler_login_destroy(login);
	rpchandler_signals_destroy(signals);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	obstack_free(&obstack, NULL);
	return ec;
}
