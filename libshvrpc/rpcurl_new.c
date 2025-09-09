#include <stdlib.h>
#include <shv/rpctransport.h>
#include <shv/rpcurl.h>


rpcclient_t rpcurl_new_client(const struct rpcurl *url) {
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
		case RPC_PROTOCOL_CAN:
			return rpcclient_can_new(
				url->location, url->port, url->can.local_address);
		default:
			abort(); // GCOVR_EXCL_LINE
	}
}

rpcserver_t rpcurl_new_server(const struct rpcurl *url) {
	switch (url->protocol) {
		case RPC_PROTOCOL_TCP:
			return rpcserver_tcp_new(url->location, url->port, RPCSTREAM_P_BLOCK);
		case RPC_PROTOCOL_TCPS:
			return rpcserver_tcp_new(url->location, url->port, RPCSTREAM_P_SERIAL);
		case RPC_PROTOCOL_UNIX:
			return rpcserver_unix_new(url->location, RPCSTREAM_P_BLOCK);
		case RPC_PROTOCOL_UNIXS:
			return rpcserver_unix_new(url->location, RPCSTREAM_P_SERIAL);
		case RPC_PROTOCOL_TTY:
			return rpcserver_tty_new(
				url->location, url->tty.baudrate, RPCSTREAM_P_SERIAL_CRC);
		case RPC_PROTOCOL_CAN:
			return rpcserver_can_new(url->location, url->port);
		default:
			abort(); // GCOVR_EXCL_LINE
	}
}
