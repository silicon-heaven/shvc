#include <shv/rpcclient.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <shv/rpcmsg.h>
#include "sha1.h"



static void sha1_password(const char *nonce, const char *password, char *res) {
	sha1ctx_t ctx = sha1_new();
	assert(ctx);
	assert(sha1_update(ctx, (const uint8_t *)nonce, strlen(nonce)));
	assert(sha1_update(ctx, (const uint8_t *)password, strlen(password)));
	assert(sha1_hex_digest(ctx, res));
	sha1_destroy(ctx);
}


bool rpcclient_login(
	rpcclient_t client, const struct rpclogin_options *opts, char **errmsg) {
	bool res = false;
	if (errmsg)
		*errmsg = NULL;
	struct obstack obs;
	obstack_init(&obs);
	void *obs_base = obstack_base(&obs);
	struct cpitem item;
	cpitem_unpack_init(&item);
	struct rpcmsg_meta meta;
	char *nonce = NULL;

	/* Hello */
	rpcmsg_pack_request(rpcclient_pack(client), NULL, "hello", 1);
	cp_pack_null(rpcclient_pack(client));
	cp_pack_container_end(rpcclient_pack(client));
	if (!rpcclient_sendmsg(client))
		goto err;

	/* Response to hello */
	if (!(rpcclient_nextmsg(client) &&
			rpcmsg_head_unpack(rpcclient_unpack(client), &item, &meta, NULL, &obs) &&
			meta.request_id == 1))
		goto err;
	obstack_free(&obs, obs_base);
	if (cp_unpack_type(rpcclient_unpack(client), &item) != CPITEM_MAP)
		goto err;
	for_cp_unpack_map(rpcclient_unpack(client), &item) {
		char str[7];
		if (cp_unpack_strncpy(rpcclient_unpack(client), &item, str, 7) < 7 &&
			!strcmp(str, "nonce")) {
			nonce = cp_unpack_strdup(rpcclient_unpack(client), &item);
			continue;
		}
		cp_unpack_skip(rpcclient_unpack(client), &item);
	}
	if (!rpcclient_validmsg(client))
		goto err;

	/* Login */
	rpcmsg_pack_request(rpcclient_pack(client), NULL, "login", 2);
	cp_pack_map_begin(rpcclient_pack(client));

	cp_pack_str(rpcclient_pack(client), "login");
	cp_pack_map_begin(rpcclient_pack(client));
	cp_pack_str(rpcclient_pack(client), "password");
	if (opts->login_type == RPC_LOGIN_SHA1) {
		char sha1[SHA1_HEX_SIZ];
		sha1_password(nonce, opts->password, sha1);
		cp_pack_string(rpcclient_pack(client), sha1, SHA1_HEX_SIZ);
	} else
		cp_pack_str(rpcclient_pack(client), opts->password);
	cp_pack_str(rpcclient_pack(client), "user");
	cp_pack_str(rpcclient_pack(client), opts->username);
	cp_pack_str(rpcclient_pack(client), "type");
	switch (opts->login_type) {
		case RPC_LOGIN_PLAIN:
			cp_pack_str(rpcclient_pack(client), "PLAIN");
			break;
		case RPC_LOGIN_SHA1:
			cp_pack_str(rpcclient_pack(client), "SHA1");
			break;
	}
	cp_pack_container_end(rpcclient_pack(client));

	if (opts->device_id || opts->device_mountpoint) {
		cp_pack_str(rpcclient_pack(client), "options");
		cp_pack_map_begin(rpcclient_pack(client));
		cp_pack_str(rpcclient_pack(client), "device");
		cp_pack_map_begin(rpcclient_pack(client));
		if (opts->device_id) {
			cp_pack_str(rpcclient_pack(client), "deviceId");
			cp_pack_str(rpcclient_pack(client), opts->device_id);
		}
		if (opts->device_mountpoint) {
			cp_pack_str(rpcclient_pack(client), "mountPoint");
			cp_pack_str(rpcclient_pack(client), opts->device_mountpoint);
		}
		cp_pack_container_end(rpcclient_pack(client));
		cp_pack_container_end(rpcclient_pack(client));
	}

	cp_pack_container_end(rpcclient_pack(client));
	cp_pack_container_end(rpcclient_pack(client));
	if (!rpcclient_sendmsg(client))
		goto err;

	/* Response to login */
	if (!(rpcclient_nextmsg(client) &&
			rpcmsg_head_unpack(rpcclient_unpack(client), &item, &meta, NULL, &obs) &&
			meta.request_id == 2))
		goto err;
	if (errmsg && meta.type == RPCMSG_T_ERROR)
		rpcmsg_unpack_error(rpcclient_unpack(client), &item, NULL, errmsg);
	if (!rpcclient_validmsg(client))
		goto err;
	res = meta.type == RPCMSG_T_RESPONSE;
err:
	free(nonce);
	obstack_free(&obs, NULL);
	return res;
}
