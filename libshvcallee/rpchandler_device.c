#include <stdlib.h>
#include <sys/sysinfo.h>
#include <shv/rpchandler_device.h>
#include <shv/rpchandler_impl.h>

#include "rpchandler_device_method.gperf.h"

struct rpchandler_device {
	const char *name;
	const char *version;
	const char *serial_number;
	void (*reset)(void);
};


static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	if (ctx->path[0] == '\0')
		rpchandler_ls_result_const(ctx, ".device");
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	if (strcmp(ctx->path, ".device"))
		return;

	if (ctx->name) { /* Faster match against gperf */
		if (gperf_rpchandler_device_method(ctx->name, strlen(ctx->name)))
			rpchandler_dir_exists(ctx);
		return;
	}

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
			.name = "serialNumber",
			.result = "String",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_BROWSE,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "uptime",
			.result = "UInt",
			.flags = RPCDIR_F_GETTER,
			.access = RPCACCESS_READ,
		});
	rpchandler_dir_result(ctx,
		&(const struct rpcdir){
			.name = "reset",
			.access = RPCACCESS_COMMAND,
		});
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_device *rpchandler_device = cookie;
	if (ctx->meta.type != RPCMSG_T_REQUEST || strcmp(ctx->meta.path, ".device"))
		return RPCHANDLER_MSG_SKIP;
	const struct gperf_rpchandler_device_method_match *match =
		gperf_rpchandler_device_method(ctx->meta.method, strlen(ctx->meta.method));
	if (match == NULL)
		return RPCHANDLER_MSG_SKIP;

	bool has_param = rpcmsg_has_value(ctx->item);
	if (!rpchandler_msg_valid(ctx))
		return RPCHANDLER_MSG_DONE;

	if (has_param) {
		rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM, "Must be only 'null'");
		return RPCHANDLER_MSG_DONE;
	}

	cp_pack_t pack;
	switch (match->method) {
		case M_NAME:
			pack = rpchandler_msg_new_response(ctx);
			cp_pack_str(pack, rpchandler_device->name);
			rpchandler_msg_send_response(ctx, pack);
			break;
		case M_VERSION:
			pack = rpchandler_msg_new_response(ctx);
			cp_pack_str(pack, rpchandler_device->version);
			rpchandler_msg_send_response(ctx, pack);
			break;
		case M_SERIAL_NUMBER:
			pack = rpchandler_msg_new_response(ctx);
			cp_pack_str(pack, rpchandler_device->serial_number);
			rpchandler_msg_send_response(ctx, pack);
			break;
		case M_UPTIME: {
			if (!rpchandler_msg_access_level(ctx, RPCACCESS_READ))
				break;
			struct sysinfo si;
			sysinfo(&si);
			pack = rpchandler_msg_new_response(ctx);
			cp_pack_int(pack, si.uptime);
			rpchandler_msg_send_response(ctx, pack);
			break;
		}
		case M_RESET:
			if (!rpchandler_msg_access_level(ctx, RPCACCESS_COMMAND))
				break;
			if (rpchandler_device->reset) {
				rpchandler_msg_send_response_void(ctx);
				rpchandler_device->reset();
			} else
				rpchandler_msg_send_error(
					ctx, RPCERR_NOT_IMPLEMENTED, "Reset is not available");
			break;
	}
	return RPCHANDLER_MSG_DONE;
}

static const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};

rpchandler_device_t rpchandler_device_new(const char *name, const char *version,
	const char *serial_number, void (*reset)(void)) {
	rpchandler_device_t res = malloc(sizeof *res);
	*res = (struct rpchandler_device){
		.name = name,
		.version = version,
		.serial_number = serial_number,
		.reset = reset,
	};
	return res;
}

void rpchandler_device_destroy(rpchandler_device_t rpchandler_device) {
	free(rpchandler_device);
}

struct rpchandler_stage rpchandler_device_stage(
	rpchandler_device_t rpchandler_device) {
	return (struct rpchandler_stage){
		.funcs = &rpc_funcs, .cookie = rpchandler_device};
}
