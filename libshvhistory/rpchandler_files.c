#include <stdlib.h>
#include <unistd.h>
#include <shv/rpchandler_files.h>
#include <shv/rpchandler_impl.h>

struct rpchandler_files {
	const char *name;
	void *log;
	struct rpchandler_files_ops *ops;
};

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_files *handler = cookie;
	if (!ctx->path || (ctx->path[0] == '\0'))
		rpchandler_ls_result(ctx, ".files");
	if (ctx->path && !strcmp(ctx->path, ".files"))
		rpchandler_ls_result(ctx, handler->name);
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	// TODO
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	return RPCHANDLER_MSG_SKIP;
}

static const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};

rpchandler_files_t rpchandler_files_new(
	const char *name, void *log, struct rpchandler_files_ops *ops) {
	rpchandler_files_t res = malloc(sizeof *res);
	if (!res)
		return NULL;

	res->name = name;
	res->log = log;
	res->ops = ops;

	return res;
}

void rpchandler_files_destroy(rpchandler_files_t files) {
	free(files);
}

struct rpchandler_stage rpchandler_files_stage(rpchandler_files_t files) {
	return (struct rpchandler_stage){.funcs = &rpc_funcs, .cookie = files};
}
