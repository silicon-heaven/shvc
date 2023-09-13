#include <shv/rpcurl.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <uriparser/Uri.h>
#include <assert.h>

#include "rpcurl_scheme.gperf.h"
#include "rpcurl_query.gperf.h"

struct dyn_rpcurl {
	struct rpcurl rpcurl;
	/* We want to allow user to change fields including the replace. That would
	 * result in lost reference or in inability to decide if we should free it
	 * or not. We instead store all dynamically allocated strings here so we can
	 * free them when we need it.
	 */
	char *location;
	char *username;
	char *password;
	char *device_id;
	char *device_mountpoint;
};

struct rpcurl *rpcurl_parse(const char *url, const char **error_pos) {
	UriUriA uri;
	if (uriParseSingleUriA(&uri, url, error_pos) != URI_SUCCESS)
		return NULL;

#define PARSE_ERR(LOC) \
	do { \
		if (error_pos) \
			*error_pos = LOC; \
		goto error; \
	} while (false)

	struct dyn_rpcurl *res = calloc(1, sizeof *res);

	res->rpcurl.protocol = RPC_PROTOCOL_LOCAL_SOCKET;
	if (uri.scheme.first != uri.scheme.afterLast) {
		const struct gperf_rpcurl_scheme_match *match = gperf_rpcurl_scheme(
			uri.scheme.first, uri.scheme.afterLast - uri.scheme.first);
		if (match == NULL)
			PARSE_ERR(uri.scheme.first);
		res->rpcurl.protocol = match->protocol;
	}
	if (res->rpcurl.protocol == RPC_PROTOCOL_TCP ||
		res->rpcurl.protocol == RPC_PROTOCOL_UDP)
		res->rpcurl.port = 3755;

	if (uri.userInfo.first != uri.userInfo.afterLast) {
		res->username = strndup(
			uri.userInfo.first, uri.userInfo.afterLast - uri.userInfo.first);
		res->rpcurl.login.username = res->username;
	}

	if (uri.hostText.first != uri.hostText.afterLast) {
		if (res->rpcurl.protocol != RPC_PROTOCOL_TCP &&
			res->rpcurl.protocol != RPC_PROTOCOL_UDP)
			PARSE_ERR(uri.portText.first - 1);
		res->location = strndup(
			uri.hostText.first, uri.hostText.afterLast - uri.hostText.first);
		res->rpcurl.location = res->location;
	}

	if (uri.pathHead && uri.pathHead->text.first != uri.pathTail->text.afterLast) {
		if (res->rpcurl.protocol != RPC_PROTOCOL_LOCAL_SOCKET &&
			res->rpcurl.protocol != RPC_PROTOCOL_SERIAL_PORT)
			PARSE_ERR(uri.pathHead->text.first);
		assert(!uri.absolutePath || *(uri.pathHead->text.first - 1) == '/');
		size_t off = uri.absolutePath ? 1 : 0;
		res->location = strndup(uri.pathHead->text.first - off,
			uri.pathTail->text.afterLast - uri.pathHead->text.first + off);
		res->rpcurl.location = res->location;
	}

	if (uri.portText.first != uri.portText.afterLast) {
		if (res->rpcurl.protocol != RPC_PROTOCOL_TCP &&
			res->rpcurl.protocol != RPC_PROTOCOL_UDP)
			PARSE_ERR(uri.portText.first - 1);
		int base = 1;
		res->rpcurl.port = 0;
		for (const char *c = uri.portText.afterLast - 1;
			 c >= uri.portText.first; c--) {
			if (*c < '0' || *c > '9') {
				if (error_pos)
					*error_pos = c;
				goto error;
			}
			res->rpcurl.port += (*c - '0') * base;
			base *= 10;
		}
	}

	if (uri.query.first != uri.query.afterLast) {
		UriQueryListA *querylist;
		int querycnt;
		if (uriDissectQueryMallocA(&querylist, &querycnt, uri.query.first,
				uri.query.afterLast) != URI_SUCCESS)
			PARSE_ERR(uri.query.first);
		UriQueryListA *q = querylist;
		while (q != NULL) {
			const struct gperf_rpcurl_query_match *match =
				gperf_rpcurl_query(q->key, strlen(q->key));
			if (match == NULL) {
				uriFreeQueryListA(querylist);
				PARSE_ERR(uri.query.first);
			}
			switch (match->query) {
				case GPERF_RPCURL_QUERY_SHAPASS:
					free(res->password);
					res->rpcurl.login.login_type = RPC_LOGIN_SHA1;
					res->password = strdup(q->value);
					res->rpcurl.login.password = res->password;
					break;
				case GPERF_RPCURL_QUERY_PASSWORD:
					free(res->password);
					res->rpcurl.login.login_type = RPC_LOGIN_PLAIN;
					res->password = strdup(q->value);
					res->rpcurl.login.password = res->password;
					break;
				case GPERF_RPCURL_QUERY_DEVID:
					free(res->device_id);
					res->device_id = strdup(q->value);
					res->rpcurl.login.device_id = res->device_id;
					break;
				case GPERF_RPCURL_QUERY_DEVMOUNT:
					free(res->device_mountpoint);
					res->device_mountpoint = strdup(q->value);
					res->rpcurl.login.device_mountpoint = res->device_mountpoint;
					break;
			}
			q = q->next;
		}
		uriFreeQueryListA(querylist);
	}

	uriFreeUriMembersA(&uri);
	if (error_pos)
		*error_pos = NULL;
	return &res->rpcurl;

error:
	uriFreeUriMembersA(&uri);
	rpcurl_free(&res->rpcurl);
	return NULL;
}

void rpcurl_free(struct rpcurl *rpc_url) {
	struct dyn_rpcurl *url = (struct dyn_rpcurl *)rpc_url;
	free(url->location);
	free(url->username);
	free(url->password);
	free(url->device_id);
	free(url->device_mountpoint);
	free(url);
}

char *rpcurl_str(struct rpcurl *rpc_url) {
	// TODO
	return NULL;
}
