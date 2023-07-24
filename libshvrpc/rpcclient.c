#include <shv/rpcclient.h>
#include <shv/cpon.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


struct rpcclient_msg rpcclient_next_message(rpcclient_t client) {
	struct rpcclient_msg res = {
		.ctx = client->nextmsg(client),
	};
	if (res.ctx) {
		const char *t = cpcp_unpack_take_byte(res.ctx);
		if (t)
			res.format = *t;
	}
	return res;
}

const cpcp_item *rpcclient_next_item(struct rpcclient_msg msg) {
	static const cpcp_item invalid = {.type = CPCP_ITEM_INVALID};
	if (msg.ctx == NULL)
		return &invalid;
	switch (msg.format) {
		case CPCP_ChainPack:
			chainpack_unpack_next(msg.ctx);
			break;
		case CPCP_Cpon:
			cpon_unpack_next(msg.ctx);
			break;
		default:
			return &invalid;
	}
	if (msg.ctx->err_no != CPCP_RC_OK)
		return &invalid;
	return &msg.ctx->item;
}

bool rpcclient_skip_item(struct rpcclient_msg msg) {
	unsigned depth = 0;
	do {
		const cpcp_item *i = rpcclient_next_item(msg);
		switch (i->type) {
			case CPCP_ITEM_STRING:
			case CPCP_ITEM_BLOB:
				while (!i->as.String.last_chunk) {
					i = rpcclient_next_item(msg);
					assert(i->type == CPCP_ITEM_STRING ||
						i->type == CPCP_ITEM_BLOB);
				}
				break;
			case CPCP_ITEM_LIST:
			case CPCP_ITEM_MAP:
			case CPCP_ITEM_IMAP:
			case CPCP_ITEM_META:
				depth++;
				break;
			case CPCP_ITEM_CONTAINER_END:
				depth--;
				break;
			case CPCP_ITEM_INVALID:
				return false;
			default:
				break;
		}
	} while (depth);
	return true;
}

void rpcclient_disconnect(rpcclient_t client) {
	if (client == NULL || client->disconnect == NULL)
		return;
	client->disconnect(client);
}

struct rpcclient *rpcclient_connect(const struct rpcurl *url) {
	struct rpcclient *res;
	switch (url->protocol) {
		case RPC_PROTOCOL_TCP:
			res = rpcclient_stream_tcp_connect(url->location, url->port);
			break;
		case RPC_PROTOCOL_LOCAL_SOCKET:
			res = rpcclient_stream_unix_connect(url->location);
			break;
		case RPC_PROTOCOL_UDP:
			res = rpcclient_datagram_udp_connect(url->location, url->port);
			break;
		case RPC_PROTOCOL_SERIAL_PORT:
			res = rpcclient_serial_connect(url->location);
			break;
		default:
			abort();
	};
	if (rpcclient_login(res, url->username, url->password, url->login_type))
		return res;
	rpcclient_disconnect(res);
	return NULL;
}

bool rpcclient_login(rpcclient_t client, const char *username,
	const char *password, enum rpc_login_type type) {
	// TODO
	return false;
}
