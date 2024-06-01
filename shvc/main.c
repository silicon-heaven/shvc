#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shv/cp_tools.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpcmsg.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_login.h>
#include <shv/rpccall.h>
#include "opts.h"

#define ERR_COM (RPCMSG_E_USER_CODE + 1)
#define ERR_TIM (RPCMSG_E_USER_CODE + 2)
#define ERR_PARAM (RPCMSG_E_USER_CODE + 3)

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

struct ctx {
	struct conf *conf;
	FILE *fparam;
	char *output;
	int error;
};

int response_callback(enum rpccall_stage stage, cp_pack_t pack, int request_id,
	cp_unpack_t unpack, struct cpitem *item, void *ctx) {
	struct ctx *c = ctx;
	switch (stage) {
		case CALL_S_PACK: {
			if (c->fparam) {
				rpcmsg_pack_request(
					pack, c->conf->path, c->conf->method, request_id);
				fseek(c->fparam, 0, SEEK_SET);
				struct cp_unpack_cpon cp_unpack_cpon;
				cp_unpack_t cpon_unpack =
					cp_unpack_cpon_init(&cp_unpack_cpon, c->fparam);
				struct cpitem item;
				cpitem_unpack_init(&item);
				if (!cp_repack(cpon_unpack, &item, pack)) {
					c->error = ERR_PARAM;
					return 1;
				}
				free(cp_unpack_cpon.state.ctx);
				cp_pack_container_end(pack);
			} else
				rpcmsg_pack_request_void(
					pack, c->conf->path, c->conf->method, request_id);
			break;
		}
		case CALL_S_RESULT: {
			free(c->output);
			size_t outputsiz = 0;
			FILE *f = open_memstream(&c->output, &outputsiz);
			struct cp_pack_cpon cp_pack_cpon;
			cp_pack_t cpon_pack = cp_pack_cpon_init(&cp_pack_cpon, f, "\t");
			/* Note: We ignore repack error here because we can't act on it. */
			cp_repack(unpack, item, cpon_pack);
			free(cp_pack_cpon.state.ctx);
			fclose(f);
			return 0;
		}
		case CALL_S_VOID_RESULT:
			free(c->output);
			c->output = NULL;
			return 0;
		case CALL_S_ERROR: {
			rpcmsg_error err = RPCMSG_E_UNKNOWN;
			rpcmsg_unpack_error(unpack, item, &err, &c->output);
			c->error = err;
			break;
		}
		case CALL_S_COMERR:
			c->error = ERR_COM;
			break;
		case CALL_S_TIMERR:
			c->error = ERR_TIM;
			break;
	}
	return 0;
}

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
	client->logger_in =
		rpclogger_new(rpclogger_func_stderr, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(rpclogger_func_stderr, "=> ", logsiz, conf.verbose);

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_app_t app = rpchandler_app_new("shvc", PROJECT_VERSION);
	rpchandler_responses_t responses = rpchandler_responses_new();
	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(app),
		rpchandler_responses_stage(responses),
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);

	pthread_t handler_thread;
	assert(!rpchandler_spawn_thread(handler, &handler_thread, NULL));

	int ec = 0;

	rpcmsg_error login_err;
	const char *login_errmsg;
	if (!rpchandler_login_wait(login, &login_err, &login_errmsg, NULL)) {
		ec = 3;
		fprintf(stderr, "Failed to login to: %s\n", conf.url);
		fprintf(stderr, "%s\n", login_errmsg);
		goto cleanup;
	}

	struct ctx ctx;
	ctx.conf = &conf;
	ctx.output = NULL;
	ctx.error = RPCMSG_E_NO_ERROR;

	uint8_t *stdin_param = NULL;
	size_t stdin_param_siz = 0;
	if (conf.stdin_param) {
		ctx.fparam = open_memstream((char **)&stdin_param, &stdin_param_siz);
		{
			char buf[BUFSIZ];
			size_t rsiz;
			while ((rsiz = fread(buf, 1, BUFSIZ, stdin)))
				fwrite(buf, rsiz, 1, ctx.fparam);
		}
	} else if (conf.param)
		ctx.fparam = fmemopen((void *)conf.param, strlen(conf.param), "r");
	else
		ctx.fparam = NULL;

	rpccall(handler, responses, response_callback, &ctx);

	if (ctx.fparam)
		fclose(ctx.fparam);
	free(stdin_param);

	switch (ctx.error) {
		case RPCMSG_E_NO_ERROR:
			if (ctx.output)
				puts(ctx.output);
			else
				puts("null");
			break;
		case ERR_PARAM:
			fprintf(stderr, "Invalid CPON provided as parameter\n");
			ec = -2;
			break;
		case ERR_COM:
			fprintf(stderr, "Communication error\n");
			ec = -3;
			break;
		case ERR_TIM:
			fprintf(stderr, "Communication timeout\n");
			ec = -4;
			break;
		default:
			fprintf(stderr, "SHV Error: %s\n", ctx.output);
			ec = ctx.error;
			break;
	}
	free(ctx.output);

cleanup:
	pthread_cancel(handler_thread);
	pthread_join(handler_thread, NULL);
	rpchandler_destroy(handler);
	rpchandler_app_destroy(app);
	rpchandler_login_destroy(login);
	rpchandler_responses_destroy(responses);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	rpcurl_free(rpcurl);
	return ec;
}
