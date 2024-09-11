#include <stdlib.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_impl.h>
#include <shv/version.h>

#include "app_method.gperf.h"

struct rpchandler_app {
	const char *name;
	const char *version;
};


static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	if (ctx->path[0] == '\0')
		rpchandler_ls_result_const(ctx, ".app");
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	if (strcmp(ctx->path, ".app"))
		return;

	if (ctx->name) { /* Faster match against gperf */
		if (gperf_rpchandler_app_method(ctx->name, strlen(ctx->name)))
			rpchandler_dir_exists(ctx);
		return;
	}

	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "shvVersionMajor",
			.result = "Int",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_BROWSE,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "shvVersionMinor",
			.result = "Int",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_BROWSE,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "name",
			.result = "String",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_BROWSE,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "version",
			.result = "String",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_BROWSE,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "ping",
			.access = RPCACCESS_BROWSE,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "date",
			.result = "DateTime",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_BROWSE,
		});
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_app *rpchandler_app = cookie;
	if (ctx->meta.type != RPCMSG_T_REQUEST || strcmp(ctx->meta.path, ".app"))
		return RPCHANDLER_MSG_SKIP;
	const struct gperf_rpchandler_app_method_match *match =
		gperf_rpchandler_app_method(ctx->meta.method, strlen(ctx->meta.method));
	if (match == NULL)
		return RPCHANDLER_MSG_SKIP;
	if (rpchandler_msg_valid_nullparam(ctx))
		switch (match->method) {
			case M_SHV_VERSION_MAJOR: {
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_int(pack, SHV_VERSION_MAJOR);
				rpchandler_msg_send_response(ctx, pack);
				break;
			}
			case M_SHV_VERSION_MINOR: {
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_int(pack, SHV_VERSION_MINOR);
				rpchandler_msg_send_response(ctx, pack);
				break;
			}
			case M_NAME: {
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_str(pack, rpchandler_app->name ?: "shvc");
				rpchandler_msg_send_response(ctx, pack);
				break;
			}
			case M_VERSION: {
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_str(pack, rpchandler_app->version ?: PROJECT_VERSION);
				rpchandler_msg_send_response(ctx, pack);
				break;
			}
			case M_PING:
				rpchandler_msg_send_response_void(ctx);
				break;
			case M_DATE: {
				cp_pack_t pack = rpchandler_msg_new_response(ctx);
				cp_pack_datetime(pack, clock_cpdatetime());
				rpchandler_msg_send_response(ctx, pack);
				break;
			}
		}
	return RPCHANDLER_MSG_DONE;
}

static const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};


rpchandler_app_t rpchandler_app_new(const char *name, const char *version) {
	rpchandler_app_t res = malloc(sizeof *res);
	*res = (struct rpchandler_app){
		.name = name,
		.version = version,
	};
	return res;
}

void rpchandler_app_destroy(rpchandler_app_t rpchandler_app) {
	free(rpchandler_app);
}

struct rpchandler_stage rpchandler_app_stage(rpchandler_app_t rpchandler_app) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = rpchandler_app};
}
