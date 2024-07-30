#include <alloca.h>
#include <shv/rpclogin.h>
#include <shv/sha1.h>

bool rpclogin_pack(cp_pack_t pack, const struct rpclogin *login,
	const char *nonce, bool trusted) {
	cp_pack_map_begin(pack);

	cp_pack_str(pack, "login");
	cp_pack_map_begin(pack);
	if (!cp_pack_str(pack, "password"))
		return false; /* Just detect error earlier here */
	const char *password = login->password;
	enum rpclogin_type login_type = login->login_type;
	if (login_type == RPC_LOGIN_PLAIN && !trusted) {
		char *shapass = alloca(SHA1_HEX_SIZ + 1);
		sha1_hex(
			(const uint8_t *)login->password, strlen(login->password), shapass);
		shapass[SHA1_HEX_SIZ] = '\0';
		password = shapass;
		login_type = RPC_LOGIN_SHA1;
	}
	switch (login_type) {
		case RPC_LOGIN_PLAIN:
			cp_pack_str(pack, login->password);
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
		default:
			return false;
	}
	cp_pack_str(pack, "user");
	cp_pack_str(pack, login->username);
	cp_pack_str(pack, "type");
	cp_pack_str(pack, login_type == RPC_LOGIN_PLAIN ? "PLAIN" : "SHA1");
	cp_pack_container_end(pack);

	if (login->device_id || login->device_mountpoint) {
		cp_pack_str(pack, "options");
		cp_pack_map_begin(pack);
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
		cp_pack_container_end(pack);
	}

	return cp_pack_container_end(pack);
}
