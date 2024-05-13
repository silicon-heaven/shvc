#include <stdio.h>
#include <stdlib.h>
#include <shv/rpcprotocol.h>
#include <assert.h>
#include <errno.h>
#include "rpcprotocol_common.h"


void set_error(rpcprotocol_private *protocol, const int errnum) {
	protocol->public_interface.errnum = errnum;
}

void flushmsg(struct rpcprotocol_interface *protocol_interface) {
	/* Flush the rest of the message. Read up to the EOF. */
	char buf[BUFSIZ];
	while (fread(buf, 1, BUFSIZ, protocol_interface->message_stream) > 0) {}
}

size_t fread_raw_bytes(
	rpcprotocol_private *protocol, char *buf, size_t length, FILE *filestream) {
	size_t bytes_read = 0;

	do {
		clearerr(filestream);
		bytes_read = fread(buf, SINGLE_BYTE, length, filestream);
	} while (ferror(filestream) && (errno == EINTR));

	if (feof(filestream))
		return 0;

	if (ferror(filestream))
		set_error(protocol, errno);

	return bytes_read;
}

int16_t fread_single_raw_byte(rpcprotocol_private *protocol, FILE *filestream) {
	uint8_t byte;

	if (fread_raw_bytes(protocol, (void *)&byte, SINGLE_BYTE, filestream) !=
		SINGLE_BYTE) {
		set_error(protocol, ENODATA);
		return FAILURE;
	}

	return byte;
}

bool fwrite_raw_bytes(rpcprotocol_private *protocol, const void *buf,
	size_t size, FILE *filestream) {
	const uint8_t *data = (const uint8_t *)buf;

	while (size > 0) {
		size_t bytes_written = fwrite(data, SINGLE_BYTE, size, filestream);
		if (ferror(filestream)) {
			if (errno != EINTR) {
				set_error(protocol, errno);
				return false;
			}
		} else {
			data += bytes_written;
			size -= bytes_written;
		}
	}

	fflush(filestream);

	return true;
}

bool fwrite_single_raw_byte(
	rpcprotocol_private *protocol, const uint8_t byte, FILE *filestream) {
	return fwrite_raw_bytes(protocol, (const void *)&byte, SINGLE_BYTE, filestream);
}

void destroy_protocol(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;
	bool should_free_wbuf =
		protocol->public_interface.trans_info->transport_layer == STREAM;

	free(protocol->public_interface.trans_info->destination.name);
	free(protocol->public_interface.trans_info);
	fclose(protocol->public_interface.byte_stream);
	fclose(protocol->public_interface.message_stream);
	if (should_free_wbuf)
		free(protocol->layers.stream.wbuf);
	free(protocol);
}
