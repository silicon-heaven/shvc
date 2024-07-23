#include <shv/rpcclient_url.h>
#include <shv/rpcclient_stream.h>
#include <stdlib.h>

#define DEFAULT_BAUDRATE 115200


rpcclient_t rpcclient_new(const struct rpcurl *url) {
	switch (url->protocol) {
		case RPC_PROTOCOL_TCP:
			return rpcclient_tcp_new(url->location, url->port, RPCSTREAM_P_BLOCK);
		case RPC_PROTOCOL_TCPS:
			return rpcclient_tcp_new(url->location, url->port, RPCSTREAM_P_SERIAL);
		case RPC_PROTOCOL_UNIX:
			return rpcclient_unix_new(url->location, RPCSTREAM_P_BLOCK);
		case RPC_PROTOCOL_UNIXS:
			return rpcclient_unix_new(url->location, RPCSTREAM_P_SERIAL);
		case RPC_PROTOCOL_TTY:
			return rpcclient_tty_new(
				url->location, url->tty.baudrate, RPCSTREAM_P_SERIAL_CRC);
		default:
			abort();
	}
}

rpcclient_t rpcclient_connect(const struct rpcurl *url) {
	rpcclient_t res = rpcclient_new(url);
	if (rpcclient_reset(res))
		return res;
	rpcclient_destroy(res);
	return NULL;
}
