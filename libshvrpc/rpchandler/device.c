#include <stdlib.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <shv/rpcalerts.h>
#include <shv/rpchandler_device.h>
#include <shv/rpchandler_impl.h>

#include "device_method.gperf.h"

struct rpchandler_device_alerts {
	cp_pack_t pack;
};

struct rpchandler_device {
	const char *name;
	const char *version;
	const char *serial_number;
	void (*alerts)(rpchandler_device_alerts_t);
	void (*reset)(void);
};

void rpchandler_device_signal_alerts(
	rpchandler_device_t rpchandler_device, rpchandler_t handler) {
	// TODO Right now there is not unified API to send signals (either broker or
	// handler would have to be used here)
}

bool rpchandler_device_alert(
	rpchandler_device_alerts_t ctx, struct rpcalerts *alert) {
	return rpcalerts_pack(ctx->pack, alert);
}

static void rpc_ls(void *cookie, struct rpchandler_ls *ctx) {
	struct rpchandler_device *rpchandler_device = cookie;
	if (ctx->path[0] == '\0')
		rpchandler_ls_result_const(ctx, ".device");
	else if (!strcmp(ctx->path, ".device") && rpchandler_device->alerts)
		rpchandler_ls_result_const(ctx, "alerts");
}

static void rpc_dir(void *cookie, struct rpchandler_dir *ctx) {
	struct rpchandler_device *rpchandler_device = cookie;
	if (!strcmp(ctx->path, ".device")) {
		if (ctx->name) { /* Faster match against gperf */
			if (gperf_rpchandler_device_method(ctx->name, strlen(ctx->name)))
				rpchandler_dir_exists(ctx);
			return;
		}

		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "name",
				.result = "s",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "version",
				.result = "s",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "serialNumber",
				.result = "s",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_BROWSE,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "uptime",
				.result = "u|n",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_READ,
			});
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "reset",
				.access = RPCACCESS_COMMAND,
			});
	} else if (!strcmp(ctx->path, ".device/alerts") && rpchandler_device->alerts) {
		rpchandler_dir_result(ctx,
			&(const struct rpcdir){
				.name = "get",
				.param = "i|n",
				.result = "[!alert]",
				.flags = RPCDIR_F_GETTER,
				.access = RPCACCESS_READ,
				.signals = (const struct rpcdirsig[]){{.name = "chng"}},
				.signals_cnt = 1,
			});
	}
}

static enum rpchandler_msg_res rpc_msg(void *cookie, struct rpchandler_msg *ctx) {
	struct rpchandler_device *rpchandler_device = cookie;
	if (ctx->meta.type != RPCMSG_T_REQUEST)
		return RPCHANDLER_MSG_SKIP;
	if (!strcmp(ctx->meta.path, ".device")) {
		const struct gperf_rpchandler_device_method_match *match =
			gperf_rpchandler_device_method(
				ctx->meta.method, strlen(ctx->meta.method));
		if (match != NULL)
			switch (match->method) {
				case M_NAME:
					/* No need to check for browse level access */
					if (rpchandler_msg_valid_nullparam(ctx)) {
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						cp_pack_str(pack, rpchandler_device->name);
						rpchandler_msg_send_response(ctx, pack);
					}
					return RPCHANDLER_MSG_DONE;
				case M_VERSION:
					/* No need to check for browse level access */
					if (rpchandler_msg_valid_nullparam(ctx)) {
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						cp_pack_str(pack, rpchandler_device->version);
						rpchandler_msg_send_response(ctx, pack);
					}
					return RPCHANDLER_MSG_DONE;
				case M_SERIAL_NUMBER:
					/* No need to check for browse level access */
					if (rpchandler_msg_valid_nullparam(ctx)) {
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						cp_pack_str(pack, rpchandler_device->serial_number);
						rpchandler_msg_send_response(ctx, pack);
					}
					return RPCHANDLER_MSG_DONE;
				case M_UPTIME:
					if (ctx->meta.access < RPCACCESS_READ)
						break;
					if (rpchandler_msg_valid_nullparam(ctx)) {
						struct sysinfo si;
						sysinfo(&si);
						cp_pack_t pack = rpchandler_msg_new_response(ctx);
						cp_pack_int(pack, si.uptime);
						rpchandler_msg_send_response(ctx, pack);
					} else
						rpchandler_msg_send_method_not_found(ctx);
					return RPCHANDLER_MSG_DONE;
				case M_RESET:
					if (ctx->meta.access < RPCACCESS_COMMAND)
						break;
					if (rpchandler_msg_valid_nullparam(ctx)) {
						if (rpchandler_device->reset) {
							rpchandler_msg_send_response_void(ctx);
							rpchandler_device->reset();
						} else
							rpchandler_msg_send_error(ctx,
								RPCERR_NOT_IMPLEMENTED, "Reset is not available");
					}
					return RPCHANDLER_MSG_DONE;
			}
	} else if (!strcmp(ctx->meta.path, ".device/alerts") &&
		rpchandler_device->alerts) {
		if (!strcmp(ctx->meta.method, "get")) {
			if (ctx->meta.access < RPCACCESS_READ)
				return RPCHANDLER_MSG_DONE;
			if (rpchandler_msg_valid_nullparam(ctx)) {
				struct rpchandler_device_alerts alerts;
				alerts.pack = rpchandler_msg_new_response(ctx);
				cp_pack_list_begin(alerts.pack);
				rpchandler_device->alerts(&alerts);
				cp_pack_container_end(alerts.pack);
				rpchandler_msg_send_response(ctx, alerts.pack);
			}
			return RPCHANDLER_MSG_DONE;
		}
	}
	return RPCHANDLER_MSG_SKIP;
}

static const struct rpchandler_funcs rpc_funcs = {
	.ls = rpc_ls,
	.dir = rpc_dir,
	.msg = rpc_msg,
};

rpchandler_device_t rpchandler_device_new(const char *name, const char *version,
	const char *serial_number, void (*alerts)(rpchandler_device_alerts_t),
	void (*reset)(void)) {
	rpchandler_device_t res = malloc(sizeof *res);
	*res = (struct rpchandler_device){
		.name = name,
		.version = version,
		.serial_number = serial_number,
		.alerts = alerts,
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
