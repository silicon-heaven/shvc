#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <uriparser/Uri.h>
#include <shv/rpcurl.h>

#include "rpcurl_query.gperf.h"
#include "rpcurl_scheme.gperf.h"

#define DEFAULT_PROTOCOL (RPC_PROTOCOL_UNIX)
#define DEFAULT_BAUDRATE (115200)

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

inline static bool is_uri_segment_present(const UriTextRangeA *segment) {
	return (segment->first != segment->afterLast);
}

inline static size_t uri_segment_len(const UriTextRangeA *segment) {
	return (segment->afterLast - segment->first);
}

/* Thanks to Boolean algebra rules, the __negation__ of this check is equal to
 * protocol != TCP && protocol != TCPS.
 */
inline static bool protocol_contains_authority(const struct rpcurl *res) {
	return (res->protocol == RPC_PROTOCOL_TCP ||
		res->protocol == RPC_PROTOCOL_TCPS ||
		res->protocol == RPC_PROTOCOL_SSL || res->protocol == RPC_PROTOCOL_SSLS);
}

inline static bool protocol_must_contain_path(const struct rpcurl *res) {
	return ((res->protocol == RPC_PROTOCOL_UNIX) ||
		(res->protocol == RPC_PROTOCOL_UNIXS) ||
		(res->protocol == RPC_PROTOCOL_TTY));
}

static inline char *strdupo(struct obstack *obstack, const char *str) {
	return obstack_copy0(obstack, str, strlen(str));
}

static bool parse_query(const UriUriA *uri, struct rpcurl *res,
	struct obstack *obstack, const char **error_pos) {
	if (!is_uri_segment_present(&uri->query))
		return true;

	UriQueryListA *querylist;
	int querycnt;
	if (uriDissectQueryMallocA(&querylist, &querycnt, uri->query.first,
			uri->query.afterLast) != URI_SUCCESS) {
		*error_pos = uri->query.first;
		return false;
	}

#define PARSE_ERR(LOC) \
	do { \
		uriFreeQueryListA(querylist); \
		*error_pos = LOC; \
		return false; \
	} while (false)

	UriQueryListA *q = querylist;
	while (q != NULL) {
		const struct gperf_rpcurl_query_match *match =
			gperf_rpcurl_query(q->key, strlen(q->key));
		if (match == NULL)
			PARSE_ERR(uri->query.first);
		switch (match->query) {
			case GPERF_RPCURL_QUERY_USER:
				res->login.username = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_SHAPASS:
				res->login.login_type = RPC_LOGIN_SHA1;
				res->login.password = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_PASSWORD:
				res->login.login_type = RPC_LOGIN_PLAIN;
				res->login.password = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_DEVID:
				res->login.device_id = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_DEVMOUNT:
				res->login.device_mountpoint = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_CA:
				if (res->protocol != RPC_PROTOCOL_SSL &&
					res->protocol != RPC_PROTOCOL_SSLS)
					PARSE_ERR(uri->query.first);
				res->ssl.ca = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_CERT:
				if (res->protocol != RPC_PROTOCOL_SSL &&
					res->protocol != RPC_PROTOCOL_SSLS)
					PARSE_ERR(uri->query.first);
				res->ssl.cert = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_KEY:
				if (res->protocol != RPC_PROTOCOL_SSL &&
					res->protocol != RPC_PROTOCOL_SSLS)
					PARSE_ERR(uri->query.first);
				res->ssl.key = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_CRL:
				if (res->protocol != RPC_PROTOCOL_SSL &&
					res->protocol != RPC_PROTOCOL_SSLS)
					PARSE_ERR(uri->query.first);
				res->ssl.crl = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_VERIFY:
				if (res->protocol != RPC_PROTOCOL_SSL &&
					res->protocol != RPC_PROTOCOL_SSLS)
					PARSE_ERR(uri->query.first);
				if (!strcasecmp(q->value, "true")) {
					res->ssl.verify = true;
				} else if (!strcasecmp(q->value, "false")) {
					res->ssl.verify = false;
				} else
					// TODO we should point to the value here
					PARSE_ERR(uri->query.first);
				break;
			case GPERF_RPCURL_QUERY_BAUDRATE:
				if (res->protocol != RPC_PROTOCOL_TTY) {
					uriFreeQueryListA(querylist);
					PARSE_ERR(uri->query.first);
				}
				char *end;
				res->tty.baudrate = strtol(q->value, &end, 10);
				if (*end != '\0')
					PARSE_ERR(uri->query.first);
				break;
		}
		q = q->next;
	}
	uriFreeQueryListA(querylist);
	return true;
}


struct rpcurl *rpcurl_parse(
	const char *url, const char **errpos, struct obstack *obstack) {
	if (errpos)
		*errpos = NULL;

	UriUriA uri;
	if (uriParseSingleUriA(&uri, url, errpos) != URI_SUCCESS)
		return NULL;

	struct rpcurl *res = obstack_alloc(obstack, sizeof *res);
	memset(res, 0, sizeof *res);

#define ERROR(POS) \
	do { \
		if (errpos) \
			*errpos = (POS); \
		obstack_free(obstack, res); \
		res = NULL; \
		goto error; \
	} while (false)

	res->protocol = DEFAULT_PROTOCOL;
	if (is_uri_segment_present(&uri.scheme)) {
		const struct gperf_rpcurl_scheme_match *match =
			gperf_rpcurl_scheme(uri.scheme.first, uri_segment_len(&uri.scheme));
		if (match == NULL)
			ERROR(uri.scheme.first);
		res->protocol = match->protocol;
	}

	switch (res->protocol) {
		case RPC_PROTOCOL_TCP:
			res->port = RPCURL_TCP_PORT;
			break;
		case RPC_PROTOCOL_TCPS:
			res->port = RPCURL_TCPS_PORT;
			break;
		case RPC_PROTOCOL_SSL:
			res->port = RPCURL_SSL_PORT;
			break;
		case RPC_PROTOCOL_SSLS:
			res->port = RPCURL_SSLS_PORT;
			break;
		case RPC_PROTOCOL_UNIX:
			break;
		case RPC_PROTOCOL_UNIXS:
			break;
		case RPC_PROTOCOL_TTY:
			res->tty.baudrate = DEFAULT_BAUDRATE;
			break;
	}

	if (is_uri_segment_present(&uri.userInfo)) {
		res->login.username = obstack_copy0(
			obstack, uri.userInfo.first, uri_segment_len(&uri.userInfo));
	}

	if (is_uri_segment_present(&uri.hostText)) {
		if (!protocol_contains_authority(res))
			ERROR(uri.portText.first - 1);
		res->location = obstack_copy0(
			obstack, uri.hostText.first, uri_segment_len(&uri.hostText));
	}

	if (uri.pathHead && (uri.pathHead->text.first != uri.pathTail->text.afterLast)) {
		if (!protocol_must_contain_path(res))
			ERROR(uri.pathHead->text.first);
		assert(!uri.absolutePath || (*(uri.pathHead->text.first - 1) == '/'));
		size_t off = uri.absolutePath ? 1 : 0;
		res->location = obstack_copy0(obstack, uri.pathHead->text.first - off,
			uri.pathTail->text.afterLast - uri.pathHead->text.first + off);
	}

	if (is_uri_segment_present(&uri.portText)) {
		if (!protocol_contains_authority(res))
			ERROR(uri.pathHead->text.first);
		int base = 1;
		res->port = 0;
		// TODO is there really no function to convert not null terminated
		// string to number?
		for (const char *c = uri.portText.afterLast - 1;
			 c >= uri.portText.first; c--) {
			if ((*c < '0') || (*c > '9'))
				ERROR(c);
			res->port += (*c - '0') * base;
			base *= 10;
		}
	}

	if (!parse_query(&uri, res, obstack, errpos))
		res = NULL;

#undef ERROR
error:
	uriFreeUriMembersA(&uri);
	return res;
}
