#include <shv/rpcclient.h>
#include <stdlib.h>

#define DEFAULT_BAUDRATE 115200


struct rpcclient *rpcclient_connect(const struct rpcurl *url) {
	switch (url->protocol) {
		case RPC_PROTOCOL_TCP:
			return rpcclient_stream_tcp_connect(url->location, url->port);
		case RPC_PROTOCOL_TCPS:
			return rpcclient_serial_tcp_connect(url->location, url->port);
		case RPC_PROTOCOL_UNIX:
			return rpcclient_stream_unix_connect(url->location);
		case RPC_PROTOCOL_UNIXS:
			return rpcclient_serial_unix_connect(url->location);
		case RPC_PROTOCOL_TTY:
			return rpcclient_serial_tty_connect(url->location, DEFAULT_BAUDRATE);
		default:
			abort();
	}
}
