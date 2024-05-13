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
	bool nextmsg_return = nextmsg_protocol_stream(client->protocol_interface);
	if (!nextmsg_return)
		return false;

	int format_byte = fgetc(client->protocol_interface->message_stream);
	if (format_byte != CP_ChainPack) {
		flushmsg(client->protocol_interface);
		return false;
	} else {
		if (client->logger_in)
			rpclogger_log_end(client->logger_in, RPCLOGGER_ET_UNKNOWN);
		return true;
	}
}

bool msgflush_stream(rpcclient_t client) {
	if (client->logger_out)
		rpclogger_log_end(client->logger_out, RPCLOGGER_ET_INVALID);
	return msgflush_protocol_stream(client->protocol_interface);
}

bool msgsend_stream(rpcclient_t client) {
	if (client->logger_out)
		rpclogger_log_end(client->logger_out, RPCLOGGER_ET_VALID);
	return msgsend_protocol_stream(client->protocol_interface);
}

bool cp_pack_stream(void *ptr, const struct cpitem *item) {
	rpcclient_t client =
		(rpcclient_t)((char *)ptr - offsetof(struct rpcclient, pack));
	if (client->logger_out)
		rpclogger_log_item(client->logger_out, item);
	return chainpack_pack(client->protocol_interface->message_stream, item) > 0;
}

void cp_unpack_stream(void *ptr, struct cpitem *item) {
	rpcclient_t client =
		(rpcclient_t)((char *)ptr - offsetof(struct rpcclient, unpack));
	chainpack_unpack(client->protocol_interface->message_stream, item);
	if (client->logger_in)
		rpclogger_log_item(client->logger_in, item);
}

static bool validmsg_stream(rpcclient_t client) {
	flushmsg(client->protocol_interface);
	if (client->logger_in)
		rpclogger_log_end(client->logger_in, RPCLOGGER_ET_VALID);
	return true; /* Always valid for stream as we rely on lower layer */
}

static void destroy(rpcclient_t client) {
	destroy_protocol(client->protocol_interface);
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

rpcclient_t rpcclient_stream_new(
	struct rpcprotocol_interface *protocol_interface, int socket) {
	struct rpcclient *result = malloc(sizeof *result);

	*result = (struct rpcclient){.ctl = ctl_stream,
		.protocol_interface = protocol_interface,
		.unpack = cp_unpack_stream,
		.pack = cp_pack_stream,
		.fd = socket};

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
	int sock = -1;
	for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock == -1)
			continue;
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) != -1)
			break;
		close(sock);
		sock = -1;
	}
	freeaddrinfo(addrs);
	if (sock == -1)
		return NULL;

	struct rpcprotocol_info *trans_info = malloc(sizeof *trans_info);
	assert(trans_info);
	*trans_info = (struct rpcprotocol_info){
		.protocol_scheme = TCP,
		.destination =
			(struct protocol_destination){
				.name = strdup(location),
				.additional_info.port = (uint16_t)port,
			},
		.transport_layer = STREAM,
	};

	struct rpcprotocol_interface *protocol_interface =
		rpcprotocol_stream_new(sock, trans_info);
	return rpcclient_stream_new(protocol_interface, sock);
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
		.destination =
			(struct protocol_destination){
				.name = strdup(location),
			},
		.transport_layer = STREAM,
	};

	struct rpcprotocol_interface *protocol_interface =
		rpcprotocol_stream_new(sock, trans_info);
	return rpcclient_stream_new(protocol_interface, sock);
}
