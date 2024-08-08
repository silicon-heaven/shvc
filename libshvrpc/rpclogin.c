#include <stdlib.h>
#include <alloca.h>
#include <shv/rpclogin.h>
#include <shv/sha1.h>

bool rpclogin_pack(cp_pack_t pack, const struct rpclogin *login,
	const char *nonce, bool trusted) {
	cp_pack_map_begin(pack);

	if (login->username) {
		cp_pack_str(pack, "login");
		cp_pack_map_begin(pack);
		cp_pack_str(pack, "user");
		cp_pack_str(pack, login->username);
		if (!cp_pack_str(pack, "password")) // GCOVR_EXCL_BR_LINE
			return false;					/* Just detect error earlier here */
		const char *password = login->password ?: "";
		enum rpclogin_type login_type = login->login_type;
		if (login_type == RPC_LOGIN_PLAIN && !trusted) {
			char *shapass = alloca(SHA1_HEX_SIZ + 1);
			sha1_hex((const uint8_t *)password, strlen(password), shapass);
			shapass[SHA1_HEX_SIZ] = '\0';
			password = shapass;
			login_type = RPC_LOGIN_SHA1;
		}
		switch (login_type) {
			case RPC_LOGIN_PLAIN:
				cp_pack_str(pack, password);
				break;
			case RPC_LOGIN_SHA1:
				sha1ctx_t ctx = sha1_new();
				sha1_update(ctx, (const uint8_t *)nonce, strlen(nonce));
				sha1_update(ctx, (const uint8_t *)password, strlen(password));
				char res[SHA1_HEX_SIZ];
				sha1_hex_digest(ctx, res);
				sha1_destroy(ctx);
				cp_pack_string(pack, res, SHA1_HEX_SIZ);
				break;
			default:		  // GCOVR_EXCL_LINE
				return false; // GCOVR_EXCL_LINE
		}
		cp_pack_str(pack, "type");
		cp_pack_str(pack, login_type == RPC_LOGIN_PLAIN ? "PLAIN" : "SHA1");
		cp_pack_container_end(pack);
	}

	if (login->idle_timeout > 0 || login->device_id || login->device_mountpoint) {
		cp_pack_str(pack, "options");
		cp_pack_map_begin(pack);
		if (login->device_id || login->device_mountpoint) {
			cp_pack_str(pack, "device");
			cp_pack_map_begin(pack);
			if (login->device_id) {
				cp_pack_str(pack, "deviceId");
				cp_pack_str(pack, login->device_id);
			}
			if (login->device_mountpoint) {
				cp_pack_str(pack, "mountPoint");
				cp_pack_str(pack, login->device_mountpoint);
			}
			cp_pack_container_end(pack);
		}
		if (login->idle_timeout) {
			cp_pack_str(pack, "idleWatchDogTimeOut");
			cp_pack_int(pack, login->idle_timeout);
		}
		cp_pack_container_end(pack);
	}

	return cp_pack_container_end(pack);
}

struct rpclogin *rpclogin_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_type(unpack, item) != CPITEM_MAP)
		return NULL;
	struct rpclogin *res = obstack_alloc(obstack, sizeof *res);
	*res = (struct rpclogin){
		.login_type = RPC_LOGIN_PLAIN,
	};
#define FAIL \
	do { \
		obstack_free(obstack, res); \
		return NULL; \
	} while (false)
	for_cp_unpack_map(unpack, item, key, 8) {
		if (!strcmp(key, "login")) {
			if (cp_unpack_type(unpack, item) != CPITEM_MAP)
				FAIL;
			for_cp_unpack_map(unpack, item, lkey, 8) {
				if (!strcmp(lkey, "user")) {
					res->username = cp_unpack_strdupo(unpack, item, obstack);
					if (res->username == NULL)
						FAIL;
				} else if (!strcmp(lkey, "password")) {
					res->password = cp_unpack_strdupo(unpack, item, obstack);
					if (res->password == NULL)
						FAIL;
				} else if (!strcmp(lkey, "type")) {
					char *type = cp_unpack_strndup(unpack, item, 6);
					if (type == NULL)
						FAIL;
					cp_unpack_drop1(unpack, item);
					if (!strcmp(type, "PLAIN"))
						res->login_type = RPC_LOGIN_PLAIN;
					else if (!strcmp(type, "SHA1"))
						res->login_type = RPC_LOGIN_SHA1;
					else {
						free(type);
						FAIL;
					}
					free(type);
				} else
					cp_unpack_skip(unpack, item);
			}
		} else if (!strcmp(key, "options")) {
			if (cp_unpack_type(unpack, item) != CPITEM_MAP)
				FAIL;
			for_cp_unpack_map(unpack, item, okey, 20) {
				// TODO idleWatchDogTimeOut
				if (!strcmp(okey, "device")) {
					if (cp_unpack_type(unpack, item) != CPITEM_MAP)
						FAIL;
					for_cp_unpack_map(unpack, item, odkey, 11) {
						if (!strcmp(odkey, "deviceId")) {
							res->device_id =
								cp_unpack_strdupo(unpack, item, obstack);
							if (res->device_id == NULL)
								FAIL;
						} else if (!strcmp(odkey, "mountPoint")) {
							res->device_mountpoint =
								cp_unpack_strdupo(unpack, item, obstack);
							if (res->device_mountpoint == NULL)
								FAIL;
						} else
							cp_unpack_skip(unpack, item);
					}
				} else if (!strcmp(okey, "idleWatchDogTimeOut")) {
					if (!cp_unpack_int(unpack, item, res->idle_timeout))
						FAIL;
				} else
					cp_unpack_skip(unpack, item);
			}
		} else
			cp_unpack_skip(unpack, item);
	}
#undef FAIL
	return res;
}

bool rpclogin_validate_password(const struct rpclogin *login,
	const char *password, const char *nonce, enum rpclogin_type type) {
	const char *lpassword = login->password ?: "";
	if (login->login_type == RPC_LOGIN_PLAIN && type == RPC_LOGIN_SHA1) {
		/* Hash the plain password and compare hashed against hashed. */
		char *shapass = alloca(SHA1_HEX_SIZ + 1);
		sha1_hex((const uint8_t *)lpassword, strlen(lpassword), shapass);
		shapass[SHA1_HEX_SIZ] = '\0';
		lpassword = shapass;
		type = RPC_LOGIN_PLAIN;
	} else if (login->login_type == RPC_LOGIN_SHA1 && type == RPC_LOGIN_PLAIN) {
		/* Elevate to SHA1 to perform SHA1 authentication. */
		char *shapass = alloca(SHA1_HEX_SIZ + 1);
		sha1_hex((const uint8_t *)password, strlen(password), shapass);
		shapass[SHA1_HEX_SIZ] = '\0';
		password = shapass;
		type = RPC_LOGIN_SHA1;
	}
	switch (type) {
		case RPC_LOGIN_PLAIN:
			return !strcmp(password, lpassword);
		case RPC_LOGIN_SHA1:
			sha1ctx_t ctx = sha1_new();
			sha1_update(ctx, (const uint8_t *)nonce, strlen(nonce));
			sha1_update(ctx, (const uint8_t *)password, strlen(password));
			char res[SHA1_HEX_SIZ + 1];
			sha1_hex_digest(ctx, res);
			res[SHA1_HEX_SIZ] = '\0';
			sha1_destroy(ctx);
			return !strcmp(res, lpassword);
		default: // GCOVR_EXCL_LINE
			return false;
	}
}
