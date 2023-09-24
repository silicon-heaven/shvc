#include <shv/rpcapp.h>
#include <stdlib.h>

struct rpcapp {
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

static enum rpchandler_func_res rpc_msg(
	void *cookie, struct rpcreceive *receive, const struct rpcmsg_meta *meta) {
	struct rpcapp *rpcapp = cookie;
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
		return RPCHFR_UNHANDLED;

	if (!rpcreceive_validmsg(receive))
		return RPCHFR_RECV_ERR;
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
			cp_pack_str(pack, rpcapp->name ?: "shvc"); // TODO
			break;
		case APP_VERSION:
			cp_pack_str(pack, rpcapp->version ?: PROJECT_VERSION); // TODO
			break;
		case UNKNOWN:
			abort(); /* Can't happen */
	}
	cp_pack_container_end(pack);
	rpcreceive_response_send(receive);
	return RPCHFR_HANDLED;
}

const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};


rpcapp_t rpcapp_new(const char *name, const char *version) {
	rpcapp_t res = malloc(sizeof *res);
	res->name = name;
	res->version = version;
	return res;
}

void rpcapp_destroy(rpcapp_t rpcapp) {
	free(rpcapp);
}

struct rpchandler_stage rpcapp_handler_stage(rpcapp_t rpcapp) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = rpcapp};
}
