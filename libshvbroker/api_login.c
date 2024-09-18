#include <sys/random.h>
#include <shv/rpchandler_impl.h>

#include "api_login.h"
#include "shvc_config.h"

_Static_assert(NONCE_LEN % 2 == 0,
	"Must be even number due to generate_nonce implementation");
static void generate_nonce(char nonce[NONCE_LEN + 1]) {
	const char *hex = "0123456789abcdef";
	uint8_t buf[NONCE_LEN / 2];
#ifdef SHVC_CRYPTORAND
	size_t siz = 0;
	while (siz < (NONCE_LEN / 2)) {
		ssize_t cnt = getrandom(buf + siz, (NONCE_LEN / 2) - siz, GRND_RANDOM);
		assert(cnt != -1);
		siz += cnt;
	}
#else
	for (size_t i = 0; i < (NONCE_LEN / 2); i++)
		buf[i] = rand() % 0xff;
#endif
	for (int i = 0; i < (NONCE_LEN / 2); i++) {
		nonce[i * 2] = hex[buf[i] & 0xf];
		nonce[i * 2 + 1] = hex[(buf[i] >> 4) & 0xf];
	}
	nonce[NONCE_LEN] = '\0';
}

void rpcbroker_api_login_msg(struct clientctx *c, struct rpchandler_msg *ctx) {
	if (ctx->meta.path == NULL || *ctx->meta.path == '\0') {
		if (!strcmp(ctx->meta.method, "hello")) {
			if (!rpchandler_msg_valid(ctx))
				return;
			if (c->nonce[0] == '\0')
				generate_nonce(c->nonce);
			cp_pack_t pack = rpchandler_msg_new(ctx);
			rpcmsg_pack_response(pack, &ctx->meta);
			cp_pack_map_begin(pack);
			cp_pack_str(pack, "nonce");
			cp_pack_string(pack, c->nonce, NONCE_LEN);
			cp_pack_container_end(pack);
			cp_pack_container_end(pack);
			rpchandler_msg_send(ctx);
			return;
		} else if (c->nonce[0] != '\0' && !strcmp(ctx->meta.method, "login")) {
			struct rpclogin *l =
				rpclogin_unpack(ctx->unpack, ctx->item, rpchandler_obstack(ctx));
			if (!rpchandler_msg_valid(ctx))
				return;
			if (l != NULL) {
				struct rpcbroker_login_res res =
					c->broker->login(c->broker->login_cookie, l, c->nonce);
				if (!res.success) {
					rpchandler_msg_send_error(ctx, RPCERR_METHOD_CALL_EXCEPTION,
						res.errmsg ?: "Invalid login");
					return;
				}
				broker_lock(c->broker);
				enum role_res role_res = role_assign(c, res.role);
				broker_unlock(c->broker);
				switch (role_res) {
					case ROLE_RES_OK:
						break;
					case ROLE_RES_MNT_INVALID:
						rpchandler_msg_send_error(ctx, RPCERR_METHOD_CALL_EXCEPTION,
							"Mount point not allowed");
						return;
					case ROLE_RES_MNT_EXISTS:
						rpchandler_msg_send_error(ctx, RPCERR_METHOD_CALL_EXCEPTION,
							"Mount point already exists");
						return;
				}
				if (l->username)
					c->username = strdup(l->username);
				c->activity_timeout = l->idle_timeout ?: SHV_IDLE_TIMEOUT_DEFAULT;
				rpchandler_msg_send_response_void(ctx);
			} else
				rpchandler_msg_send_error(ctx, RPCERR_INVALID_PARAM, NULL);
			return;
		}
	}
	if (rpchandler_msg_valid(ctx))
		rpchandler_msg_send_error(ctx, RPCERR_LOGIN_REQUIRED,
			c->nonce[0] == '\0' ? "Only method 'hello' is allowed"
								: "Only methods 'hello' and 'login' are allowed");
}
