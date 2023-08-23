#include <shv/rpcclient.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


void rpcclient_log_lock(struct rpcclient_logger *logger, bool in) {
	if (logger == NULL)
		return;
	// TODO signal interrupt
	sem_wait(&logger->semaphore);
	fputs(in ? "<= " : "=> ", logger->f);
}

void rpcclient_log_item(struct rpcclient_logger *logger, const struct cpitem *item) {
	if (logger == NULL)
		return;
	cpon_pack(logger->f, &logger->cpon_state, item);
}

void rpcclient_log_unlock(struct rpcclient_logger *logger) {
	if (logger == NULL)
		return;
	fputc('\n', logger->f);
	sem_post(&logger->semaphore);
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
