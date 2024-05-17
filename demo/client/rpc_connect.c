#include "rpc_connect.h"
#include <shv/rpcurl.h>

rpcclient_t rpc_connect(const struct conf *conf) {
	const char *errpos;
	struct rpcurl *url = rpcurl_parse(conf->url, &errpos);
	if (url == NULL) {
		fprintf(stderr, "Error: Invalid URL: '%s'.\n", conf->url);
		/* The number 21 in this case refers to the number of chars that precede
		 * the string format specifier on the line above to print URL */
		size_t offset = errpos ? errpos - conf->url : 0;
		fprintf(stderr, "%*s^\n", 21 + (unsigned)offset, "");
		return NULL;
	}

	rpcclient_t res = rpcclient_connect(url);
	if (res == NULL) {
		fprintf(stderr, "Error: Connection failed to: '%s'.\n", conf->url);
		fprintf(stderr, "Hint: Make sure broker is running and URL is correct.\n");
		rpcurl_free(url);
		return NULL;
	}

	if (conf->verbose > 0)
		res->logger = rpclogger_new(stderr, conf->verbose);

	char *loginerr = NULL;
	bool login = rpcclient_login(res, &url->login, &loginerr);
	rpcurl_free(url);
	if (!login) {
		if (loginerr != NULL && *loginerr) {
			fprintf(stderr, "Error: Login failed to: '%s'.\n", conf->url);
			fprintf(stderr, "Error: %s\n", loginerr);
		} else {
			fprintf(stderr, "Error: Failed to communicate with server.\n");
		}
		rpclogger_destroy(res->logger);
		rpcclient_destroy(res);
		return NULL;
	}

	return res;
}
