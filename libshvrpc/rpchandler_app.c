#include <shv/rpchandler_app.h>
#include <stdlib.h>

struct rpchandler_app {
	const char *name;
	const char *version;
};


static void rpc_ls(void *cookie, const char *path, struct rpchandler_ls_ctx *ctx) {
	if (path == NULL || *path == '\0')
		rpchandler_ls_result(ctx, ".app");
}


static void rpc_dir(void *cookie, const char *path, struct rpchandler_dir_ctx *ctx) {
	if (!strcmp(path, ".app")) {
		rpchandler_dir_result(ctx, "shvVersionMajor", RPCNODE_DIR_RET_VOID,
			RPCNODE_DIR_F_GETTER, RPCMSG_ACC_BROWSE, NULL);
		rpchandler_dir_result(ctx, "shvVersionMinor", RPCNODE_DIR_RET_VOID,
			RPCNODE_DIR_F_GETTER, RPCMSG_ACC_BROWSE, NULL);
		rpchandler_dir_result(ctx, "appName", RPCNODE_DIR_RET_VOID,
			RPCNODE_DIR_F_GETTER, RPCMSG_ACC_BROWSE, NULL);
		rpchandler_dir_result(ctx, "appVersion", RPCNODE_DIR_RET_VOID,
			RPCNODE_DIR_F_GETTER, RPCMSG_ACC_BROWSE, NULL);
	}
}

static bool rpc_msg(
	void *cookie, struct rpcreceive *receive, const struct rpcmsg_meta *meta) {
	struct rpchandler_app *rpchandler_app = cookie;
	enum methods {
		UNKNOWN,
		SHV_VERSION_MAJOR,
		SHV_VERSION_MINOR,
		APP_NAME,
		APP_VERSION,
	} method = UNKNOWN;
	if (meta->path && !strcmp(meta->path, ".app")) {
		if (!strcmp(meta->method, "shvVersionMajor"))
			method = SHV_VERSION_MAJOR;
		else if (!strcmp(meta->method, "shvVersionMinor"))
			method = SHV_VERSION_MINOR;
		else if (!strcmp(meta->method, "appName"))
			method = APP_NAME;
		else if (!strcmp(meta->method, "appVersion"))
			method = APP_VERSION;
	}
	if (method == UNKNOWN)
		return false;

	// TODO possibly be pedantic about not having parameter
	if (!rpcreceive_validmsg(receive))
		return true;
	cp_pack_t pack = rpcreceive_response_new(receive);
	rpcmsg_pack_response(pack, meta);
	switch (method) {
		case SHV_VERSION_MAJOR:
			cp_pack_int(pack, 3);
			break;
		case SHV_VERSION_MINOR:
			cp_pack_int(pack, 0);
			break;
		case APP_NAME:
			cp_pack_str(pack, rpchandler_app->name ?: "shvc"); // TODO
			break;
		case APP_VERSION:
			cp_pack_str(pack, rpchandler_app->version ?: PROJECT_VERSION); // TODO
			break;
		case UNKNOWN:
			abort(); /* Can't happen */
	}
	cp_pack_container_end(pack);
	rpcreceive_response_send(receive);
	return true;
}

const struct rpchandler_funcs rpc_funcs = {
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
