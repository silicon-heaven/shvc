#include <shv/rpcclient.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <netdb.h>
#include <sys/un.h>
#include "rpcprotocol_serial.h"
#include "rpcprotocol_common.h"

bool nextmsg_serial(rpcclient_t client) {
	bool nextmsg_return = nextmsg_protocol_serial(client->protocol_interface);
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

bool validmsg_serial(rpcclient_t client) {
	flushmsg(client->protocol_interface);
	if (client->logger_in)
		rpclogger_log_end(client->logger_in, RPCLOGGER_ET_VALID);
	return validmsg_protocol_serial(client->protocol_interface);
}

bool cp_pack_serial(void *ptr, const struct cpitem *item) {
	rpcclient_t client =
		(rpcclient_t)((char *)ptr - offsetof(struct rpcclient, pack));

	if (!is_sending_msg_in_progress(client->protocol_interface)) {
		fputc(CP_ChainPack, client->protocol_interface->message_stream);
	}

	if (client->logger_out)
		rpclogger_log_item(client->logger_out, item);
	return chainpack_pack(client->protocol_interface->message_stream, item) > 0;
}

void cp_unpack_serial(void *ptr, struct cpitem *item) {
	rpcclient_t client =
		(rpcclient_t)((char *)ptr - offsetof(struct rpcclient, unpack));
	chainpack_unpack(client->protocol_interface->message_stream, item);
	if (client->logger_in)
		rpclogger_log_item(client->logger_in, item);
}

bool msgsend_serial(rpcclient_t client) {
	if (client->logger_out)
		rpclogger_log_end(client->logger_out, RPCLOGGER_ET_VALID);
	return msgsend_protocol_serial(client->protocol_interface);
}

bool msgabort_serial(rpcclient_t client) {
	if (client->logger_out)
		rpclogger_log_end(client->logger_out, RPCLOGGER_ET_INVALID);
	return abort_message(client->protocol_interface);
}

static void destroy(rpcclient_t client) {
	destroy_protocol(client->protocol_interface);
	free(client);
}

int ctl_serial(rpcclient_t client, enum rpcclient_ctlop op) {
	switch (op) {
		case RPCC_CTRLOP_DESTROY:
			destroy(client);
			return true;
		case RPCC_CTRLOP_ERRNO:
			return client->protocol_interface->errnum;
		case RPCC_CTRLOP_NEXTMSG:
			return nextmsg_serial(client);
		case RPCC_CTRLOP_VALIDMSG:
			return validmsg_serial(client);
		case RPCC_CTRLOP_SENDMSG:
			return msgsend_serial(client);
		case RPCC_CTRLOP_DROPMSG:
			return msgabort_serial(client);
		case RPCC_CTRLOP_POLLFD:
			return client->fd;
	}
	abort(); /* This should not happen -> implementation error */
}

rpcclient_t rpcclient_serial_new(
	struct rpcprotocol_interface *protocol_interface, int fd) {
	struct rpcclient *result = malloc(sizeof *result);

	*result = (struct rpcclient){
		.ctl = ctl_serial,
		.protocol_interface = protocol_interface,
		.unpack = cp_unpack_serial,
		.pack = cp_pack_serial,
		.fd = fd,
	};

	return result;
}

rpcclient_t rpcclient_serial_tcp_connect(const char *location, int port) {
	struct addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	struct addrinfo *addrs;
	char *p;
	assert(asprintf(&p, "%d", port) != FAILURE);
	int res = getaddrinfo(location, p, &hints, &addrs);
	free(p);
	if (res != 0)
		return NULL;

	struct addrinfo *addr;
	int sock = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);
	for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock == FAILURE)
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
		.destination =
			(struct protocol_destination){
				.name = strdup(location),
				.additional_info.port = (uint16_t)port,
			},
		.transport_layer = SERIAL,
	};

	struct rpcprotocol_interface *protocol_interface =
		rpcprotocol_serial_new(sock, trans_info, false);
	return addr ? rpcclient_serial_new(protocol_interface, sock) : NULL;
}


rpcclient_t rpcclient_serial_unix_connect(const char *location) {
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	const size_t addrlen =
		sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;
	strncpy(addr.sun_path, location, addrlen);
	addr.sun_path[addrlen] = '\0';
	if (connect(sock, &addr, sizeof addr) == FAILURE) {
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
		.transport_layer = SERIAL,
	};

	struct rpcprotocol_interface *protocol_interface =
		rpcprotocol_serial_new(sock, trans_info, false);
	return rpcclient_serial_new(protocol_interface, sock);
}

rpcclient_t rpcclient_serial_tty_connect(const char *path, unsigned baudrate) {
	/* Open the serial device with these flags:
	 * O_RDWR for read/write access
	 * O_NOCTTY to prevent the device from becoming the controlling terminal
	 * O_NDELAY to open the device in non-blocking mode
	 */
	int fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == FAILURE) {
		perror("open");
		return NULL;
	}

	/* Configure the serial port */
	struct termios options;
	tcgetattr(fd, &options);

	/* Set the baudrate for both input and output */
	cfsetispeed(&options, baudrate);
	cfsetospeed(&options, baudrate);

	/* Control modes flag */
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB; /* Disable parity checking */
	options.c_cflag &= ~CSTOPB; /* One stop bit */
	options.c_cflag &= ~CSIZE;	/* Clear the data bits size */
	options.c_cflag |= CS8;		/* Set the data bits size to 8 */
	options.c_cflag |= CRTSCTS; /* Enable hardware flow control */

	/* Input modes flag */
	options.c_iflag &= ~(IXON | IXOFF | IXANY); /* Disable software flow control
												 */

	/* Output modes flag */
	options.c_oflag &= ~OPOST; /* Disable output processing */

	/* Local modes flag */
	/* ~ICANON to disable canonical mode and enable raw mode
	 * ~ECHO, ~ECHOE to disable echo and erasure
	 * ~ISIG disable signal generation
	 */
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	tcsetattr(fd, TCSANOW, &options);

	struct rpcprotocol_info *trans_info = malloc(sizeof *trans_info);
	assert(trans_info);
	*trans_info = (struct rpcprotocol_info){
		.protocol_scheme = TTY,
		.destination =
			(struct protocol_destination){
				.name = strdup(path),
				.additional_info.baudrate = baudrate,
			},
		.transport_layer = SERIAL,
	};

	struct rpcprotocol_interface *protocol_interface =
		rpcprotocol_serial_new(fd, trans_info, true);
	return rpcclient_serial_new(protocol_interface, fd);
}
