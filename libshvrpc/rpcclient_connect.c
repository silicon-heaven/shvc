#include <shv/rpcclient.h>
#include <stdlib.h>


struct rpcclient *rpcclient_connect(const struct rpcurl *url) {
	switch (url->protocol) {
		case RPC_PROTOCOL_TCP:
			return rpcclient_stream_tcp_connect(url->location, url->port);
		case RPC_PROTOCOL_LOCAL_SOCKET:
			return rpcclient_stream_unix_connect(url->location);
		case RPC_PROTOCOL_UDP:
			return rpcclient_datagram_udp_connect(url->location, url->port);
		case RPC_PROTOCOL_SERIAL_PORT:
			return rpcclient_serial_connect(url->location);
		default:
			abort();
	};
}
