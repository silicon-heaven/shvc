#ifndef SHV_RPCCLIENT_H
#define SHV_RPCCLIENT_H

#include <stdbool.h>
#include <shv/chainpack.h>
#include <shv/rpcurl.h>

struct rpcclient {
	cpcp_unpack_context *(*nextmsg)(struct rpcclient *);
	cpcp_pack_context *(*packmsg)(struct rpcclient *, cpcp_pack_context *prev);
	void (*disconnect)(struct rpcclient *);
};
typedef struct rpcclient *rpcclient_t;


struct rpcclient_msg {
	cpcp_unpack_context *ctx;
	enum cpcp_format format;
};

struct rpcclient_msg rpcclient_next_message(rpcclient_t) __attribute__((nonnull));

/*! Provide access to the next unpacked item.
 *
 * The item is kept valid until next call to this function or client disconnect.
 *
 * @param msg: message handle returned by `rpcclient_next_message`.
 * @returns: pointer to the item. It always returns some item but it might not
 * be a pointer to the `msg.ctx->item`. That happens when this function reports
 * errors by returning invalid item.
 */
const cpcp_item *rpcclient_next_item(struct rpcclient_msg)
	__attribute__((returns_nonnull));

/*! Instead of getting next item this skips it. The difference between getting
 * and skipping the item is that this skips the whole item. That means the whole
 * list or map as well as whole string or blob. This includes multiple calls to
 * the `rpcclient_next_item`.
 *
 * @param msg: message handle returned by `rpcclient_next_message`.
 * @returns: `false` in case an invalid item is encountered, otherwise `true`.
 */
bool rpcclient_skip_item(struct rpcclient_msg);

#define rpcclient_for_message(client) \
	for (cpcp_pack_context *pack = client->packmsg(client, NULL); pack; \
		 pack = client->packmsg(client, pack))

void rpcclient_disconnect(rpcclient_t);


rpcclient_t rpcclient_connect(const struct rpcurl *url);

rpcclient_t rpcclient_stream_new(int readfd, int writefd);
rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port);
rpcclient_t rpcclient_stream_unix_connect(const char *location);

rpcclient_t rpcclient_datagram_neq(void);
rpcclient_t rpcclient_datagram_udp_connect(const char *location, int port);

rpcclient_t rpcclient_serial_new(int fd);
rpcclient_t rpcclient_serial_connect(const char *path);

bool rpcclient_login(rpcclient_t, const char *username, const char *password,
	enum rpc_login_type type) __attribute__((nonnull(1)));


#endif
