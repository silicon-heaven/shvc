#include "rpc_connection.h"

bool parse_rpcurl(const char *url, struct rpcurl **out_rpcurl) {
	if (out_rpcurl == NULL) {
		fprintf(stderr, "Error: RPC URL output parameter is NULL.\n");
		return false;
	}

	const char *errpos;
	*out_rpcurl = rpcurl_parse(url, &errpos);

	if (*out_rpcurl != NULL)
		return true;

	int offset = errpos ? (int)(errpos - url) : 0;
	fprintf(stderr, "Error: Invalid URL: '%s'.\n", url);
	/* The number 21 in this case refers to the number of chars that precede
	 * the string format specifier on the line above to print URL */
	fprintf(stderr, "%*s^\n", 21 + offset, "");
	return false;
}

bool connect_to_rpcurl(const struct rpcurl *rpcurl, rpcclient_t *out_client) {
	*out_client = rpcclient_connect(rpcurl);

	if (*out_client != NULL)
		return true;

	fprintf(stderr, "Error: Connection failed to URL: '%s'.\n", rpcurl->location);
	fprintf(stderr, "Hint: Make sure broker is running and URL is correct.\n");
	return false;
}

bool login_with_rpcclient(rpcclient_t client, const struct rpcurl *rpcurl) {
	char *loginerr = NULL;
	if (!rpcclient_login(client, &rpcurl->login, &loginerr)) {
		if (loginerr != NULL && *loginerr) {
			fprintf(stderr, "Error: Invalid login credentials for URL: '%s'.\n",
				rpcurl->location);
			fprintf(stderr, "Error: %s\n", loginerr);
		} else {
			fprintf(stderr, "Error: Failed to communicate with server.\n");
		}

		return false;
	}

	return true;
}
