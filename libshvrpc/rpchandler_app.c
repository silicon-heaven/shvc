#include <shv/rpchandler_app.h>
#include <shv/rpchandler_impl.h>
#include <stdlib.h>

struct rpchandler_app {
	const char *name;
	const char *version;
};


static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	if (ctx->path && ctx->path[0] == '\0')
		rpchandler_ls_result(ctx, ".app");
}


static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	if (ctx->path && !strcmp(ctx->path, ".app")) {
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
	}
}

static bool rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_app *rpchandler_app = cookie;
	enum methods {
		UNKNOWN,
		SHV_VERSION_MAJOR,
		SHV_VERSION_MINOR,
		APP_NAME,
		APP_VERSION,
	} method = UNKNOWN;
	if (ctx->meta.path && !strcmp(ctx->meta.path, ".app")) {
		if (!strcmp(ctx->meta.method, "shvVersionMajor"))
			method = SHV_VERSION_MAJOR;
		else if (!strcmp(ctx->meta.method, "shvVersionMinor"))
			method = SHV_VERSION_MINOR;
		else if (!strcmp(ctx->meta.method, "name"))
			method = APP_NAME;
		else if (!strcmp(ctx->meta.method, "version"))
			method = APP_VERSION;
	}
	if (method == UNKNOWN)
		return false;
	// TODO possibly be pedantic about not having parameter
	if (!rpchandler_msg_valid(ctx))
		return true;

	cp_pack_t pack = rpchandler_msg_new_response(ctx);
	switch (method) {
		case SHV_VERSION_MAJOR:
			cp_pack_int(pack, 3);
			break;
		case SHV_VERSION_MINOR:
			cp_pack_int(pack, 0);
			break;
		case APP_NAME:
			cp_pack_str(pack, rpchandler_app->name ?: "shvc");
			break;
		case APP_VERSION:
			cp_pack_str(pack, rpchandler_app->version ?: PROJECT_VERSION);
			break;
		case UNKNOWN:
			abort(); /* Can't happen */
	}
	cp_pack_container_end(pack);
	rpchandler_msg_send(ctx);
	return true;
}

static const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};


rpchandler_app_t rpchandler_app_new(const char *name, const char *version) {
	rpchandler_app_t res = malloc(sizeof *res);
	res->name = name;
	res->version = version;
	return res;
}

void rpchandler_app_destroy(rpchandler_app_t rpchandler_app) {
	free(rpchandler_app);
}

struct rpchandler_stage rpchandler_app_stage(rpchandler_app_t rpchandler_app) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = rpchandler_app};
}
