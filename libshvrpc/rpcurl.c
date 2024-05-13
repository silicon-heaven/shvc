#include <shv/rpcurl.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <uriparser/Uri.h>
#include <assert.h>

#include "rpcurl_scheme.gperf.h"
#include "rpcurl_query.gperf.h"

#define DEFAULT_PROTOCOL RPC_PROTOCOL_UNIX
#define DEFAULT_PORT 3755
#define DECIMAL_BASE 10

#define PARSE_ERR(LOC) \
	do { \
		if (error_pos) \
			*error_pos = LOC; \
		return false; \
	} while (false)

#define ERR_CHECK(VAL) \
	do { \
		if (!(VAL)) \
			goto error; \
	} while (false)


/* Example URIs:
 *
 *      userinfo    host   port
 *       ┌──┴──┐ ┌────┴───┐┌┴─┐
 * tcp://test123@localhost:3775/forum/questions/?password=pass&devid=42
 * └┬┘   └──────────┬─────────┘└───────┬───────┘ └──────────┬─────────┘
 * scheme       authority             path                query
 * (protocol)
 *
 * unixs:/run/shvbroker.sock?user=test123&devmount=test/shvc
 * └─┬─┘ └────────┬────────┘ └──────────────┬──────────────┘
 * scheme        path                     query
 * (protocol)
 */

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

inline static bool is_uri_segment_present(const UriTextRangeA *segment) {
	return (segment->first != segment->afterLast);
}

inline static size_t uri_segment_len(const UriTextRangeA *segment) {
	return (segment->afterLast - segment->first);
}

/* Thanks to Boolean algebra rules, the __negation__ of this check is equal to
 * protocol != TCP && protocol != TCPS.
 */
inline static bool protocol_contains_authority(const struct dyn_rpcurl *res) {
	return ((res->rpcurl.protocol == RPC_PROTOCOL_TCP) ||
		(res->rpcurl.protocol == RPC_PROTOCOL_TCPS));
}

inline static bool protocol_must_contain_path(const struct dyn_rpcurl *res) {
	return ((res->rpcurl.protocol == RPC_PROTOCOL_UNIX) ||
		(res->rpcurl.protocol == RPC_PROTOCOL_UNIXS) ||
		(res->rpcurl.protocol == RPC_PROTOCOL_TTY));
}

static bool parse_protocol(
	const UriUriA *uri, struct dyn_rpcurl *res, const char **error_pos) {
	res->rpcurl.protocol = DEFAULT_PROTOCOL;

	if (is_uri_segment_present(&uri->scheme)) {
		const struct gperf_rpcurl_scheme_match *match = gperf_rpcurl_scheme(
			uri->scheme.first, uri_segment_len(&uri->scheme));
		if (match == NULL)
			PARSE_ERR(uri->scheme.first);
		res->rpcurl.protocol = match->protocol;
	}

	return true;
}

static void parse_user_info(const UriUriA *uri, struct dyn_rpcurl *res) {
	if (is_uri_segment_present(&uri->userInfo)) {
		res->username =
			strndup(uri->userInfo.first, uri_segment_len(&uri->userInfo));
		res->rpcurl.login.username = res->username;
	}
}

static bool parse_host(
	const UriUriA *uri, struct dyn_rpcurl *res, const char **error_pos) {
	if (is_uri_segment_present(&uri->hostText)) {
		if (!protocol_contains_authority(res))
			PARSE_ERR(uri->portText.first - 1);
		res->location =
			strndup(uri->hostText.first, uri_segment_len(&uri->hostText));
		res->rpcurl.location = res->location;
	}

	return true;
}

static bool parse_path(
	const UriUriA *uri, struct dyn_rpcurl *res, const char **error_pos) {
	if (uri->pathHead &&
		(uri->pathHead->text.first != uri->pathTail->text.afterLast)) {
		if (!protocol_must_contain_path(res))
			PARSE_ERR(uri->pathHead->text.first);
		assert(!uri->absolutePath || (*(uri->pathHead->text.first - 1) == '/'));
		size_t off = uri->absolutePath ? 1 : 0;
		res->location = strndup(uri->pathHead->text.first - off,
			uri->pathTail->text.afterLast - uri->pathHead->text.first + off);
		res->rpcurl.location = res->location;
	}

	return true;
}

static bool parse_port(
	const UriUriA *uri, struct dyn_rpcurl *res, const char **error_pos) {
	if (is_uri_segment_present(&uri->portText)) {
		if (!protocol_contains_authority(res))
			PARSE_ERR(uri->portText.first - 1);
		int base = 1;
		res->rpcurl.port = 0;
		for (const char *c = uri->portText.afterLast - 1;
			 c >= uri->portText.first; c--) {
			if ((*c < '0') || (*c > '9')) {
				PARSE_ERR(c);
			}
			res->rpcurl.port += (*c - '0') * base;
			base *= DECIMAL_BASE;
		}
	}

	return true;
}

static bool parse_query(
	const UriUriA *uri, struct dyn_rpcurl *res, const char **error_pos) {
	if (is_uri_segment_present(&uri->query)) {
		UriQueryListA *querylist;
		int querycnt;
		if (uriDissectQueryMallocA(&querylist, &querycnt, uri->query.first,
				uri->query.afterLast) != URI_SUCCESS)
			PARSE_ERR(uri->query.first);
		UriQueryListA *q = querylist;
		while (q != NULL) {
			const struct gperf_rpcurl_query_match *match =
				gperf_rpcurl_query(q->key, strlen(q->key));
			if (match == NULL) {
				uriFreeQueryListA(querylist);
				PARSE_ERR(uri->query.first);
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

	return true;
}

struct rpcurl *rpcurl_parse(const char *url, const char **error_pos) {
	UriUriA uri;
	if (uriParseSingleUriA(&uri, url, error_pos) != URI_SUCCESS)
		return NULL;

	struct dyn_rpcurl *res = calloc(1, sizeof *res);
	assert(res);

	ERR_CHECK(parse_protocol(&uri, res, error_pos));

	if (protocol_contains_authority(res))
		res->rpcurl.port = DEFAULT_PORT;

	parse_user_info(&uri, res);
	ERR_CHECK(parse_host(&uri, res, error_pos));
	ERR_CHECK(parse_path(&uri, res, error_pos));
	ERR_CHECK(parse_port(&uri, res, error_pos));
	ERR_CHECK(parse_query(&uri, res, error_pos));

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
