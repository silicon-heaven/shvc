#include <stdlib.h>
#include <limits.h>
#include <sys/epoll.h>
#include <shv/rpcbroker.h>
#include <shv/rpchandler_app.h>
#include <shv/rpcri.h>
#include <shv/rpctransport.h>
#include "config.h"
#include "opts.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#define aprintf(...) \
	({ \
		int siz = snprintf(NULL, 0, __VA_ARGS__) + 1; \
		char *res = alloca(siz); \
		snprintf(res, siz, __VA_ARGS__); \
		res; \
	})

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;
static volatile sig_atomic_t halt = false;

struct ctx {
	struct opts opts;
	struct config *conf;
	struct rpchandler_app_conf *app_conf;
};

static void sigint_handler(int status) {
	halt = true;
}

static bool rpcri_match_oneof(char **ris, const char *path, const char *method) {
	if (ris)
		for (char **ri = ris; *ri; ri++)
			if (rpcri_match(*ri, path, method, NULL))
				return true;
	return false;
}

static bool rpcpath_match_oneof(char **patterns, const char *path) {
	if (patterns)
		for (char **pattern = patterns; *pattern; pattern++)
			if (rpcpath_match(*pattern, path))
				return true;
	return false;
}

static rpcaccess_t rpcaccess(void *cookie, const char *path, const char *method) {
	char ***ri_access = cookie;
	if (ri_access)
		for (rpcaccess_t access = RPCACCESS_ADMIN; access > 0; access--)
			if (rpcri_match_oneof(ri_access[access], path, method))
				return access;
	return RPCACCESS_NONE;
}

static void free_role(struct rpcbroker_role *role) {
	free((char *)role->mount_point);
	free(role);
};

static struct rpcbroker_login_res login(
	void *cookie, const struct rpclogin *login, const char *nonce) {
	struct ctx *ctx = cookie;
	struct user *user = config_get_user(ctx->conf, login->username);
	if (user == NULL ||
		!rpclogin_validate_password(login, user->password, nonce, user->login_type))
		return (struct rpcbroker_login_res){false, .errmsg = NULL};

	char *mount_point = NULL;
	struct role *role = config_get_role(ctx->conf, user->role);
	if (login->device_mountpoint) {
		if (rpcpath_match_oneof(role->ri_mount_points, login->device_mountpoint))
			mount_point = strdup(login->device_mountpoint);
		else
			return (struct rpcbroker_login_res){
				false, .errmsg = "Mount point not allowed"};
	}

	struct autosetup *autosetup =
		config_get_autosetup(ctx->conf, login->device_id, user->role);
	if (autosetup) {
		if (!mount_point && autosetup->mount_point)
			mount_point = strdup(autosetup->mount_point);
	}

	struct rpcbroker_role *res = malloc(sizeof *res);
	*res = (struct rpcbroker_role){
		.name = role->name,
		.access = rpcaccess,
		.access_cookie = role->ri_access,
		.mount_point = mount_point,
		.subscriptions = autosetup ? (const char **)autosetup->subscriptions : NULL,
		.free = free_role,
	};
	return (struct rpcbroker_login_res){true, .role = res};
}

static int new_client(
	void *cookie, rpcbroker_t broker, rpcserver_t server, rpcclient_t client) {
	struct ctx *ctx = cookie;
	size_t peernamelen = rpcclient_peername(client, NULL, 0);
	char peername[peernamelen + 1];
	rpcclient_peername(client, peername, peernamelen + 1);
	if (ctx->opts.verbose > 0) {
		char *lin = aprintf("%s<= ", peername);
		char *lout = aprintf("%s=> ", peername);
		client->logger_in = rpclogger_new(
			&rpclogger_stderr_funcs, lin, logsiz, ctx->opts.verbose);
		client->logger_out = rpclogger_new(
			&rpclogger_stderr_funcs, lout, logsiz, ctx->opts.verbose);
	}
	struct rpchandler_stage *stages = malloc(4 * sizeof *stages);
	stages[1] = rpchandler_app_stage(ctx->app_conf);
	stages[3] = (struct rpchandler_stage){};
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	int cid =
		rpcbroker_client_register(broker, handler, &stages[0], &stages[2], NULL);
	fprintf(stderr, "New client [%d]: %s\n", cid, peername);
	return cid;
}

static void del_client(void *cookie, rpcbroker_t broker, int cid) {
	fprintf(stderr, "Disconnected client [%d]\n", cid);
	rpchandler_t handler = rpcbroker_client_handler(broker, cid);
	rpcbroker_client_unregister(broker, cid);
	rpcclient_t client = rpchandler_client(handler);
	free((struct rpchandler_stage *)rpchandler_stages(handler));
	rpchandler_destroy(handler);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
}

int main(int argc, char **argv) {
	struct ctx ctx;
	parse_opts(argc, argv, &ctx.opts);
	struct obstack obstack;
	obstack_init(&obstack);
	ctx.conf = config_load(ctx.opts.config, &obstack);
	if (ctx.conf == NULL)
		return 1;

	signal(SIGINT, sigint_handler);
	signal(SIGHUP, sigint_handler);
	signal(SIGTERM, sigint_handler);
	int ec = 1;

	struct rpcbroker_state bstate;
	bstate.name = ctx.conf->name;
	bstate.new_client = new_client;
	bstate.del_client = del_client;
	bstate.cookie = &ctx;

	ctx.app_conf = &(struct rpchandler_app_conf){
		.name = "shvcbroker", .version = PROJECT_VERSION};
	bstate.broker = rpcbroker_new(ctx.conf->name, login, &ctx, RPCBROKER_F_NOLOCK);

	bstate.servers = calloc(ctx.conf->listen_cnt, sizeof *bstate.servers);
	bstate.servers_cnt = ctx.conf->listen_cnt;
	for (size_t i = 0; i < ctx.conf->listen_cnt; i++) {
		size_t siz = rpcurl_str(ctx.conf->listen[i], NULL, 0);
		char str[siz + 1];
		rpcurl_str(ctx.conf->listen[i], str, siz + 1);
		if ((bstate.servers[i] = rpcurl_new_server(ctx.conf->listen[i])) == NULL) {
			fprintf(stderr, "Server creation failed for %s: %m\n", str);
			goto err_server;
		} else
			fprintf(stderr, "Listening on: %s\n", str);
	}

	fprintf(stderr, "SHV RPC Broker is running.\n");
	rpcbroker_run(&bstate, &halt);
	fprintf(stderr, "SHV RPC Broker is terminating.\n");

	ec = 0;
err_server:
	for (size_t i = 0; i < ctx.conf->listen_cnt; i++)
		if (bstate.servers[i])
			rpcserver_destroy(bstate.servers[i]);
	free(bstate.servers);
	rpcbroker_destroy(bstate.broker);
	obstack_free(&obstack, NULL);
	return ec;
}
