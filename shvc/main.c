#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <shv/cp_unpack_pack.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_responses.h>
#include <obstack.h>
#include "opts.h"
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free


int main(int argc, char **argv) {
	struct conf conf;
	parse_opts(argc, argv, &conf);

	const char *errpos;
	struct rpcurl *rpcurl = rpcurl_parse(conf.url, &errpos);
	if (rpcurl == NULL) {
		fprintf(stderr, "Invalid URL: %s\n", conf.url);
		fprintf(stderr, "%*.s^\n", 13 + (int)(errpos - conf.url), "");
		return 3;
	}
	rpcclient_t client = rpcclient_connect(rpcurl);
	if (client == NULL) {
		fprintf(stderr, "Failed to connect to the: %s\n", conf.url);
		fprintf(stderr, "Please check your connection to the network\n");
		rpcurl_free(rpcurl);
		return 3;
	}
	if (conf.verbose > 0)
		client->logger = rpclogger_new(stderr, conf.verbose);
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
		return 3;
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

	pthread_t handler_thread;
	assert(!rpchandler_spawn_thread(handler, NULL, &handler_thread, NULL));

	int ec = 1;

	rpcresponse_t response;
	if (conf.param || conf.stdin_param) {
		FILE *f = conf.stdin_param
			? stdin
			: fmemopen((void *)conf.param, strlen(conf.param), "r");
		rpcresponse_send_request(handler, resps, conf.path, conf.method, response) {
			struct cp_unpack_cpon cp_cpon_unpack;
			cp_unpack_t unpack = cp_unpack_cpon_init(&cp_cpon_unpack, f);
			struct cpitem item;
			cpitem_unpack_init(&item);
			cp_unpack_pack(unpack, &item, packer);
			free(cp_cpon_unpack.state.ctx);
			fclose(f);
			// TODO we should probably check that we process all input
			switch (item.as.Error) {
				case CPERR_EOF:
				case CPERR_NONE:
					break;
				case CPERR_INVALID:
					fprintf(stderr,
						"Invalid CPON passed as parameter on byte %ld!", ftell(f));
					break;
				case CPERR_OVERFLOW:
					/* Some value is outside of our capabilities */
					fprintf(stderr,
						"The CPON parameter can't be handled by SHVC\n");
					break;
				case CPERR_IO:
					abort(); /* This should not happen */
			}
		}
	} else
		response = rpcresponse_send_request_void(
			handler, resps, conf.path, conf.method);


	if (response == NULL) {
		fprintf(stderr, "Failed to send message: %s\n", strerror(errno));
		goto err;
	}

	struct rpcreceive *receive;
	const struct rpcmsg_meta *meta;
	if (!rpcresponse_waitfor(response, &receive, &meta, conf.timeout)) {
		fprintf(stderr, "Request timed out\n");
		goto err;
	}

	struct cp_pack_cpon cp_pack_cpon;
	cp_pack_t pack = cp_pack_cpon_init(&cp_pack_cpon, stdout, "\t");
	if (rpcreceive_has_param(receive)) {
		struct cpitem item;
		cpitem_unpack_init(&item);
		cp_unpack_pack(receive->unpack, &item, pack);
	} else
		cp_pack_null(pack);
	fputc('\n', stdout);
	free(cp_pack_cpon.state.ctx);

	ec = rpcreceive_validmsg(receive) && meta->type == RPCMSG_T_RESPONSE ? 0 : ec;

err:
	// TODO rpcclient_disconnect(client); instead of cancel
	pthread_cancel(handler_thread);
	pthread_join(handler_thread, NULL);
	rpchandler_destroy(handler);
	rpchandler_app_destroy(app);
	rpchandler_responses_destroy(resps);
	rpclogger_destroy(client->logger);
	rpcclient_destroy(client);
	return ec;
}
