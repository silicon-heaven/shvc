#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_responses.h>
#include <obstack.h>
#include "opts.h"
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define TIMEOUT (-1)


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
	rpclogger_t logger = NULL;
	if (conf.verbose > 0) {
		logger = rpclogger_new(stderr, conf.verbose);
		client->logger = logger;
	}
	char *loginerr;
	if (!rpcclient_login(client, &rpcurl->login, &loginerr)) {
		if (loginerr) {
			fprintf(stderr, "Invalid login for connecting to the: %s\n", conf.url);
			fprintf(stderr, "%s\n", loginerr);
		} else {
			fprintf(stderr, "Communication error with server\n");
		}
		rpcclient_destroy(client);
		rpcurl_free(rpcurl);
		return 1;
	}
	rpcurl_free(rpcurl);

	rpchandler_app_t app = rpchandler_app_new("shvc", PROJECT_VERSION);
	rpchandler_responses_t resps = rpchandler_responses_new();

	const struct rpchandler_stage stages[] = {
		rpchandler_app_stage(app),
		rpchandler_responses_stage(resps),
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);

	int ec = 1;

	cp_pack_t pack = rpchandler_msg_new(handler);
	int rid = rpchandler_next_request_id(handler);
	if (conf.param) {
		rpcmsg_pack_request(pack, conf.path, conf.method, rid);

		FILE *f = fmemopen((void *)conf.param, strlen(conf.param), "r");
		struct cp_unpack_cpon cp_cpon_unpack;
		cp_unpack_t unpack = cp_unpack_cpon_init(&cp_cpon_unpack, f);
		struct cpitem item;
		cpitem_unpack_init(&item);
		while (cp_unpack(unpack, &item))
			cp_pack(pack, &item);
		switch (item.as.Error) {
			case CPERR_EOF:
				break; /* THis is expected exit */
			case CPERR_INVALID:
				fprintf(stderr, "Invalid CPON passed as parameter on byte %ld!",
					ftell(f));
				goto err;
			case CPERR_OVERFLOW:
				/* Some value is outside of our capabilities */
				fprintf(stderr, "The CPON parameter can't be handled by SHVC\n");
				goto err;
			case CPERR_IO:
			case CPERR_NONE:
				abort(); /* This should not happen */
		}
		free(cp_cpon_unpack.state.ctx);
		fclose(f);

		cp_pack_container_end(pack);
	} else
		rpcmsg_pack_request_void(
			rpcclient_pack(client), conf.path, conf.method, rid);

	rpcresponse_t resp = rpcresponse_expect(resps, rid);
	if (!rpchandler_msg_send(handler)) {
		fprintf(stderr, "Failed to send message: %s\n", strerror(errno));
		goto err;
	}

	struct rpcreceive *receive;
	struct rpcmsg_meta *meta;
	if (!rpcresponse_waitfor(resp, &receive, &meta, TIMEOUT)) {
		fprintf(stderr, "Request timed out\n");
		goto err;
	}

	struct obstack obs;
	obstack_init(&obs);
	void *obs_base = obstack_base(&obs);
	while (rpcclient_nextmsg(client)) {
		obstack_free(&obs, obs_base);
		struct cpitem item;
		cpitem_unpack_init(&item);
		struct rpcmsg_meta meta = (struct rpcmsg_meta){};
		if (!rpcmsg_head_unpack(rpcclient_unpack(client), &item, &meta, NULL, &obs))
			continue;
		switch (meta.type) {
			case RPCMSG_T_RESPONSE:
				break;
		}
	}
	obstack_free(&obs, NULL);

err:
	rpchandler_destroy(handler);
	rpchandler_app_destroy(app);
	rpchandler_responses_destroy(resps);
	rpcclient_destroy(client);
	if (conf.verbose > 0)
		rpclogger_destroy(logger);
	return ec;
}
