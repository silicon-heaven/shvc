#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shv/cp_tools.h>
#include <shv/rpccall.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_login.h>
#include <shv/rpcmsg.h>
#include <shv/rpcurl.h>
#include "opts.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define ERR_TIM (RPCERR_USR1)
#define ERR_PARAM (RPCERR_USR2)

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

struct ctx {
	struct conf *conf;
	FILE *fparam;
	char *output;
};

int response_callback(enum rpccall_stage stage, struct rpccall_ctx *ctx) {
	struct ctx *c = ctx->cookie;
	switch (stage) {
		case CALL_S_REQUEST:
			if (c->fparam) {
				rpcmsg_pack_request(ctx->pack, c->conf->path, c->conf->method,
					c->conf->userid, ctx->request_id);
				fseek(c->fparam, 0, SEEK_SET);
				struct cp_unpack_cpon cp_unpack_cpon;
				cp_unpack_t cpon_unpack =
					cp_unpack_cpon_init(&cp_unpack_cpon, c->fparam);
				struct cpitem item;
				cpitem_unpack_init(&item);
				if (!cp_repack(cpon_unpack, &item, ctx->pack))
					return ERR_PARAM;
				free(cp_unpack_cpon.state.ctx);
				cp_pack_container_end(ctx->pack);
			} else
				rpcmsg_pack_request_void(ctx->pack, c->conf->path,
					c->conf->method, c->conf->userid, ctx->request_id);
			free(c->output);
			c->output = NULL;
			break;
		case CALL_S_RESULT:
			size_t outputsiz = 0;
			FILE *f = open_memstream(&c->output, &outputsiz);
			struct cp_pack_cpon cp_pack_cpon;
			cp_pack_t cpon_pack = cp_pack_cpon_init(&cp_pack_cpon, f, "\t");
			/* Note: We ignore repack error here because we can't act on it. */
			cp_repack(ctx->unpack, ctx->item, cpon_pack);
			free(cp_pack_cpon.state.ctx);
			fclose(f);
			return RPCERR_NO_ERROR;
		case CALL_S_DONE:
			if (ctx->errnum != RPCERR_NO_ERROR && ctx->errmsg)
				c->output = strdup(ctx->errmsg);
			return ctx->errnum;
		case CALL_S_COMERR:
			return RPCERR_UNKNOWN;
		case CALL_S_TIMERR:
			return ERR_TIM;
	}
	return 0;
}

int main(int argc, char **argv) {
	struct conf conf;
	parse_opts(argc, argv, &conf);

	struct obstack obstack;
	obstack_init(&obstack);
	const char *errpos;
	struct rpcurl *rpcurl = rpcurl_parse(conf.url, &errpos, &obstack);
	if (rpcurl == NULL) {
		fprintf(stderr, "Invalid URL: %s\n", conf.url);
		fprintf(stderr, "%*.s^\n", 13 + (int)(errpos - conf.url), "");
		obstack_free(&obstack, NULL);
		return 3;
	}
	rpcclient_t client = rpcurl_connect_client(rpcurl);
	if (client == NULL) {
		fprintf(stderr, "Failed to connect to the: %s\n", conf.url);
		fprintf(stderr, "Please check your connection to the network\n");
		obstack_free(&obstack, NULL);
		return 3;
	}
	client->logger_in =
		rpclogger_new(&rpclogger_stderr_funcs, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(&rpclogger_stderr_funcs, "=> ", logsiz, conf.verbose);

	rpchandler_login_t login = rpchandler_login_new(&rpcurl->login);
	rpchandler_responses_t responses = rpchandler_responses_new();
	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(&(struct rpchandler_app_conf){
			.name = "shvc", .version = PROJECT_VERSION}),
		rpchandler_responses_stage(responses),
		{},
	};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	assert(handler);

	pthread_t handler_thread;
	assert(!rpchandler_spawn_thread(handler, &handler_thread, NULL));

	int ec = 0;

	rpcerrno_t login_errnum;
	const char *login_errmsg;
	if (!rpchandler_login_wait(login, &login_errnum, &login_errmsg, NULL)) {
		ec = 3;
		fprintf(stderr, "Login failure: %s\n",
			login_errmsg ?: rpcerror_str(login_errnum));
		goto cleanup;
	}

	struct ctx ctx;
	ctx.conf = &conf;
	ctx.output = NULL;

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

	ec = rpccall(handler, responses, response_callback, &ctx);

	if (ctx.fparam)
		fclose(ctx.fparam);
	free(stdin_param);

	switch (ec) {
		case RPCERR_NO_ERROR:
			if (ctx.output)
				puts(ctx.output);
			else
				puts("null");
			break;
		case ERR_PARAM:
			fprintf(stderr, "Invalid CPON provided as parameter\n");
			ec = -2;
			break;
		case ERR_TIM:
			fprintf(stderr, "Communication timeout\n");
			ec = -4;
			break;
		case RPCERR_UNKNOWN:
			fprintf(stderr, "Communication error\n");
			ec = -3;
			break;
		default:
			fprintf(stderr, "SHV Error: %s\n", ctx.output);
			break;
	}
	free(ctx.output);

cleanup:
	pthread_cancel(handler_thread);
	pthread_join(handler_thread, NULL);
	rpchandler_destroy(handler);
	rpchandler_login_destroy(login);
	rpchandler_responses_destroy(responses);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	obstack_free(&obstack, NULL);
	return ec;
}
