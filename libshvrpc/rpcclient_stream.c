#include <shv/rpcclient.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <netdb.h>
#include <sys/un.h>
#include "rpcprotocol_stream.h"
#include "rpcprotocol_common.h"


bool nextmsg_stream(rpcclient_t client) {
	int8_t nextmsg_return = nextmsg_protocol_stream(client->protocol_interface);
	if (nextmsg_return != SUCCESS)
		return false;

	int format_byte = fgetc(client->protocol_interface->message_stream);
	if (format_byte != CP_ChainPack) {
		flushmsg(client->protocol_interface);
		rpcclient_last_receive_update(client);
		return false;
	} else {
		rpclogger_log_lock(client->logger, true);
		rpcclient_last_receive_update(client);
		return true;
	}
}

bool msgflush_stream(rpcclient_t client) {
	rpclogger_log_unlock(client->logger);
	return msgflush_protocol_stream(client->protocol_interface);
}

bool msgsend_stream(rpcclient_t client) {
	rpclogger_log_unlock(client->logger);
	bool return_value = msgsend_protocol_stream(client->protocol_interface);

	if (return_value)
		rpcclient_last_send_update(client);

	return return_value;
}

bool cp_pack_stream(void *ptr, const struct cpitem *item) {
	rpcclient_t client = (rpcclient_t)((char *)ptr - offsetof(struct rpcclient, pack));
	if (ftell(client->protocol_interface->message_stream) == 0)
		rpclogger_log_lock(client->logger, false);
	rpclogger_log_item(client->logger, item);
	return chainpack_pack(client->protocol_interface->message_stream, item) > 0;
}

void cp_unpack_stream(void *ptr, struct cpitem *item) {
	rpcclient_t client = (rpcclient_t)((char *)ptr - offsetof(struct rpcclient, unpack));
	chainpack_unpack(client->protocol_interface->message_stream, item);
	rpclogger_log_item(client->logger, item);
}

static bool validmsg_stream(rpcclient_t client) {
	rpcclient_last_receive_update(client);
	rpclogger_log_unlock(client->logger);
	flushmsg(client->protocol_interface);
	rpcclient_last_receive_update(client);
	return true; /* Always valid for stream as we rely on lower layer */
}

static void destroy(rpcclient_t client) {
	destroy_protocol_stream(client->protocol_interface);
    free(client);
}

int ctl_stream(rpcclient_t client, enum rpcclient_ctlop op) {
	switch (op) {
		case RPCC_CTRLOP_DESTROY:
			destroy(client);
			return true;
		case RPCC_CTRLOP_ERRNO:
			return client->protocol_interface->errnum;
		case RPCC_CTRLOP_NEXTMSG:
			return nextmsg_stream(client);
		case RPCC_CTRLOP_VALIDMSG:
			return validmsg_stream(client);
		case RPCC_CTRLOP_SENDMSG:
			return msgsend_stream(client);
		case RPCC_CTRLOP_DROPMSG:
			return msgflush_stream(client);
		case RPCC_CTRLOP_POLLFD:
			return client->fd;
	}
	abort(); /* This should not happen -> implementation error */
}

rpcclient_t rpcclient_stream_new(struct rpcprotocol_interface *protocol_interface, int socket) {
	struct rpcclient *result = malloc(sizeof *result);

	*result = (struct rpcclient){
        .ctl = ctl_stream,
		.protocol_interface = protocol_interface,
        .unpack = cp_unpack_stream,
        .pack = cp_pack_stream,
        .logger = NULL,
		.fd = socket
	};

	return result;
}

rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port) {
	struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	struct addrinfo *addrs;
	char *p;
	assert(asprintf(&p, "%d", port) != -1);
	int res = getaddrinfo(location, p, &hints, &addrs);
	free(p);
	if (res != 0)
		return NULL;

	struct addrinfo *addr;
	int sock;
	for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock == -1)
			continue;
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) != -1)
			break;
		close(sock);
	}

	freeaddrinfo(addrs);

	struct rpcprotocol_info *trans_info = malloc(sizeof *trans_info);
	assert(trans_info);
	*trans_info = (struct rpcprotocol_info){
		.protocol_scheme = TCP,
		.destination = (struct protocol_destination){
			.name = strdup(location),
			.additional_info.port = (uint16_t)port,
		},
		.data_link = STREAM,
	};

    struct rpcprotocol_interface *protocol_interface = rpcprotocol_stream_new(sock, trans_info);
	return addr ? rpcclient_stream_new(protocol_interface, sock) : NULL;
}

rpcclient_t rpcclient_stream_unix_connect(const char *location) {
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	const size_t addrlen =
		sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;
	strncpy(addr.sun_path, location, addrlen);
	addr.sun_path[addrlen] = '\0';
	if (connect(sock, (const struct sockaddr *)&addr, sizeof addr) == -1) {
		close(sock);
		return NULL;
	}

	struct rpcprotocol_info *trans_info = malloc(sizeof *trans_info);
	assert(trans_info);
	*trans_info = (struct rpcprotocol_info){
		.protocol_scheme = UNIX,
		.destination = (struct protocol_destination){
			.name = strdup(location),
		},
		.data_link = STREAM,
	};

	struct rpcprotocol_interface *protocol_interface = rpcprotocol_stream_new(sock, trans_info);
	return rpcclient_stream_new(protocol_interface, sock);
}

