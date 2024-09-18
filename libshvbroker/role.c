#include "broker.h"
#include "mount.h"

enum role_res role_assign(struct clientctx *ctx, const struct rpcbroker_role *role) {
	enum role_res res = ROLE_RES_OK;
	ctx->role = role;
	if (role->mount_point) {
		if (*role->mount_point == '\0' ||
			is_path_prefix(role->mount_point, ".app") ||
			is_path_prefix(role->mount_point, ".broker")) {
			res = ROLE_RES_MNT_INVALID;
			goto error;
		}
		if (!mount_register(ctx)) {
			res = ROLE_RES_MNT_EXISTS;
			goto error;
		}
	}
	if (role->subscriptions)
		for (const char **ri = role->subscriptions; *ri; ri++)
			subscribe(ctx->broker, *ri, ctx->cid);

error:
	if (res != ROLE_RES_OK) {
		ctx->role = NULL;
		if (role->free)
			role->free((struct rpcbroker_role *)role);
	}
	return res;
}

void role_unassign(struct clientctx *ctx) {
	if (ctx->role == NULL)
		return;
	if (ctx->role->mount_point)
		mount_unregister(ctx);
	if (ctx->role->free)
		ctx->role->free((struct rpcbroker_role *)ctx->role);
	ctx->role = NULL;
}
