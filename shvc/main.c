#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include "opts.h"


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

	// TODO error handling
	rpcmsg_pack_request(rpcclient_pack(client), conf.path, conf.method, 3);
	// TODO pack params
	rpcclient_sendmsg(client);

	// TODO read response

	rpcclient_destroy(client);
	rpcclient_logger_destroy(logger);
	return 0;
}
