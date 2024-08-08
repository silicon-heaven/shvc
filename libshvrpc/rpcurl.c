#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <uriparser/Uri.h>
#include <shv/rpcurl.h>

#include "rpcurl_query.gperf.h"
#include "rpcurl_scheme.gperf.h"

#define DEFAULT_PROTOCOL (RPC_PROTOCOL_UNIX)
#define DEFAULT_BAUDRATE (115200)

#define aprintf(...) \
	({ \
		int siz = snprintf(NULL, 0, __VA_ARGS__) + 1; \
		char *res = alloca(siz); \
		snprintf(res, siz, __VA_ARGS__); \
		res; \
	})

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

inline static bool protocol_ssl(const struct rpcurl *res) {
	return ((res->protocol == RPC_PROTOCOL_SSL) ||
		(res->protocol == RPC_PROTOCOL_SSLS));
}

static int default_port(enum rpc_protocol protocol) {
	switch (protocol) {
		case RPC_PROTOCOL_TCP:
			return RPCURL_TCP_PORT;
		case RPC_PROTOCOL_TCPS:
			return RPCURL_TCPS_PORT;
		case RPC_PROTOCOL_SSL:
			return RPCURL_SSL_PORT;
		case RPC_PROTOCOL_SSLS:
			return RPCURL_SSLS_PORT;
		default:
			return 0;
	}
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
				if (protocol_ssl(res))
					PARSE_ERR(uri->query.first);
				res->ssl.ca = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_CERT:
				if (protocol_ssl(res))
					PARSE_ERR(uri->query.first);
				res->ssl.cert = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_KEY:
				if (protocol_ssl(res))
					PARSE_ERR(uri->query.first);
				res->ssl.key = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_CRL:
				if (protocol_ssl(res))
					PARSE_ERR(uri->query.first);
				res->ssl.crl = strdupo(obstack, q->value);
				break;
			case GPERF_RPCURL_QUERY_VERIFY:
				if (protocol_ssl(res))
					PARSE_ERR(uri->query.first);
				if (!strcasecmp(q->value, "true")) {
					res->ssl.noverify = false;
				} else if (!strcasecmp(q->value, "false")) {
					res->ssl.noverify = true;
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

	res->port = default_port(res->protocol);
	if (res->protocol == RPC_PROTOCOL_TTY)
		res->tty.baudrate = DEFAULT_BAUDRATE;

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


static inline UriTextRangeA uri_range(const char *text) {
	return (UriTextRangeA){
		.first = text,
		.afterLast = text + strlen(text),
	};
}

static UriTextRangeA protocol_str(enum rpc_protocol protocol) {
	static const char *rpcurl_scheme[] = {
		[RPC_PROTOCOL_TCP] = "tcp",
		[RPC_PROTOCOL_TCPS] = "tcps",
		[RPC_PROTOCOL_SSL] = "ssl",
		[RPC_PROTOCOL_SSLS] = "ssls",
		[RPC_PROTOCOL_UNIX] = "unix",
		[RPC_PROTOCOL_UNIXS] = "unixs",
		[RPC_PROTOCOL_TTY] = "tty",
	};
	return uri_range(rpcurl_scheme[protocol]);
}


size_t rpcurl_str(const struct rpcurl *rpcurl, char *buf, size_t size) {
	UriUriA uri;
	memset(&uri, 0, sizeof uri);
	uri.scheme = protocol_str(rpcurl->protocol);
	if (rpcurl->login.username && !strchr(rpcurl->login.username, '@'))
		uri.userInfo = uri_range(rpcurl->login.username);
	if (protocol_contains_authority(rpcurl)) {
		uri.hostText = uri_range(rpcurl->location);
		if (rpcurl->port != default_port(rpcurl->protocol))
			uri.portText = uri_range(aprintf("%d", rpcurl->port));
	} else {
		uri.pathHead = alloca(sizeof *uri.pathHead);
		uri.pathHead->text = uri_range(rpcurl->location);
		uri.pathHead->next = NULL;
		uri.pathTail = uri.pathHead;
	}

	UriQueryListA *fquery = NULL, *lquery = NULL;
#define QUERY_ADD(KEY, VAL) \
	({ \
		UriQueryListA *__new = alloca(sizeof *__new); \
		*__new = (UriQueryListA){.key = KEY, .value = VAL}; \
		if (!fquery) \
			fquery = __new; \
		if (lquery) \
			lquery->next = __new; \
		lquery = __new; \
	})
	if (rpcurl->login.username && strchr(rpcurl->login.username, '@'))
		QUERY_ADD("user", rpcurl->login.username);
	if (rpcurl->login.password) {
		switch (rpcurl->login.login_type) {
			case RPC_LOGIN_PLAIN:
				QUERY_ADD("password", rpcurl->login.password);
				break;
			case RPC_LOGIN_SHA1:
				QUERY_ADD("shapass", rpcurl->login.password);
				break;
		}
	}
	if (rpcurl->login.device_id)
		QUERY_ADD("devid", rpcurl->login.device_id);
	if (rpcurl->login.device_mountpoint)
		QUERY_ADD("devmount", rpcurl->login.device_mountpoint);
	if (protocol_ssl(rpcurl)) {
		if (rpcurl->ssl.ca)
			QUERY_ADD("ca", rpcurl->ssl.ca);
		if (rpcurl->ssl.cert)
			QUERY_ADD("cert", rpcurl->ssl.cert);
		if (rpcurl->ssl.key)
			QUERY_ADD("key", rpcurl->ssl.key);
		if (rpcurl->ssl.crl)
			QUERY_ADD("crl", rpcurl->ssl.crl);
		if (rpcurl->ssl.noverify)
			QUERY_ADD("verify", "false");
	}
	if (rpcurl->protocol == RPC_PROTOCOL_TTY)
		QUERY_ADD("baudrate", aprintf("%d", rpcurl->tty.baudrate));

	if (fquery) {
		int query_chars;
		uriComposeQueryCharsRequiredA(fquery, &query_chars);
		char *query = alloca(query_chars + 1);
		uriComposeQueryA(query, fquery, query_chars + 1, NULL);
		/* Note: query_chars for some reason calcualtes way bigger number and
		 * thus we need to calculate the correct length here.
		 */
		uri.query =
			(UriTextRangeA){.first = query, .afterLast = query + strlen(query)};
	}

	// TODO errors
	int chars;
	uriToStringCharsRequiredA(&uri, &chars);
	uriToStringA(buf, &uri, size, NULL);
	return chars;
}
