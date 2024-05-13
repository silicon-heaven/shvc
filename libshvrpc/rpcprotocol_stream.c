#include <shv/rpcclient.h>
#include <shv/rpcprotocol.h>
#include <shv/chainpack.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include "rpcprotocol_common.h"


static bool encode_and_write_message_size(
	rpcprotocol_private *protocol, size_t len) {
	unsigned num_bytes = chainpack_w_uint_bytes(len);

	for (unsigned byte_index = 0; byte_index < num_bytes; byte_index++) {
		uint8_t value = byte_index == 0
			? chainpack_w_uint_value1(len, num_bytes)
			: 0xff & (len >> (CHAR_BIT * (num_bytes - byte_index - 1)));
		if (!fwrite_raw_bytes(protocol, &value, SINGLE_BYTE,
				protocol->public_interface.byte_stream))
			return false;
	}

	return true;
}

static ssize_t receive_and_decode_message_size(rpcprotocol_private *protocol) {
	int current_byte =
		fread_single_raw_byte(protocol, protocol->public_interface.byte_stream);

	if (current_byte == FAILURE)
		return FAILURE;

	unsigned num_bytes = chainpack_int_bytes((uint8_t)current_byte);
	size_t result = chainpack_uint_value1((uint8_t)current_byte, num_bytes);

	for (unsigned byte_index = 1; byte_index < num_bytes; byte_index++) {
		current_byte = fread_single_raw_byte(
			protocol, protocol->public_interface.byte_stream);
		if (current_byte == FAILURE)
			return FAILURE;
		result = (result << CHAR_BIT) | current_byte;
	}

	return (ssize_t)result;
}

static ssize_t message_stream_write_cookie(
	void *cookie, const char *buf, size_t size) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;

	if (protocol->layers.stream.wbuf == NULL) {
		protocol->layers.stream.wbuf = malloc(size);
		assert(protocol->layers.stream.wbuf);
	}
	if (protocol->layers.stream.wbuf_capacity == 0)
		protocol->layers.stream.wbuf_capacity = size;

	size_t offset_after_write = protocol->layers.stream.wbuf_offset + size;
	size_t growth_factor =
		offset_after_write / protocol->layers.stream.wbuf_capacity;

	if ((growth_factor > 0) &&
		(protocol->layers.stream.wbuf_capacity < offset_after_write)) {
		uint8_t *new_wbuf;
		size_t capacity_after_growth =
			protocol->layers.stream.wbuf_capacity * (growth_factor + 1);
		new_wbuf = realloc(protocol->layers.stream.wbuf, capacity_after_growth);

		assert(new_wbuf);

		protocol->layers.stream.wbuf = new_wbuf;
		protocol->layers.stream.wbuf_capacity = capacity_after_growth;
	}

	memcpy(protocol->layers.stream.wbuf + protocol->layers.stream.wbuf_offset,
		buf, size);
	protocol->layers.stream.wbuf_offset = offset_after_write;
	if (protocol->layers.stream.wbuf_offset > protocol->layers.stream.wbuf_size)
		protocol->layers.stream.wbuf_size = protocol->layers.stream.wbuf_offset;

	return (ssize_t)size;
}

static ssize_t message_stream_read_cookie(void *cookie, char *buf, size_t size) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;

	if (protocol->layers.stream.rmsg_offset < size)
		/* Read up to the end of the message and then signal EOF by reading 0. */
		size = protocol->layers.stream.rmsg_offset;

	ssize_t bytes_read = (ssize_t)fread_raw_bytes(
		protocol, buf, size, protocol->public_interface.byte_stream);
	if (bytes_read > 0)
		protocol->layers.stream.rmsg_offset -= bytes_read;

	return bytes_read;
}

bool nextmsg_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	ssize_t msgsiz = receive_and_decode_message_size(protocol);
	if (msgsiz == FAILURE)
		return false;

	protocol->layers.stream.rmsg_offset = msgsiz;
	clearerr(protocol->public_interface.message_stream);

	return true;
}

bool msgsend_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	if (!encode_and_write_message_size(
			protocol, protocol->layers.stream.wbuf_size + 1))
		return false;

	uint8_t cp_flag = CP_ChainPack;
	if (!fwrite_raw_bytes(protocol, &cp_flag, SINGLE_BYTE,
			protocol->public_interface.byte_stream) ||
		!fwrite_raw_bytes(protocol, protocol->layers.stream.wbuf,
			protocol->layers.stream.wbuf_size,
			protocol->public_interface.byte_stream))
		return false;

	fseek(protocol->public_interface.message_stream, 0, SEEK_SET);

	return true;
}

bool msgflush_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	if (fseek(protocol->public_interface.message_stream, 0, SEEK_SET) == SUCCESS)
		return true;
	else
		return false;
}

int message_stream_seek_cookie(void *cookie, off_t *offset, int whence) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;

	if (whence == SEEK_SET) {
		if ((*offset < 0) || (*offset > protocol->layers.stream.wbuf_offset))
			return FAILURE;

		protocol->layers.stream.wbuf_offset = *offset;
		*offset = protocol->layers.stream.wbuf_offset;
	} else if (whence == SEEK_CUR) {
		if ((*offset + protocol->layers.stream.wbuf_offset >
				protocol->layers.stream.wbuf_size) ||
			(*offset + (ssize_t)protocol->layers.stream.wbuf_offset < 0))
			return FAILURE;

		protocol->layers.stream.wbuf_offset += *offset;
		*offset = protocol->layers.stream.wbuf_offset;
	} else if (whence == SEEK_END) {
		if (*offset > 0)
			return FAILURE;

		protocol->layers.stream.wbuf_offset =
			protocol->layers.stream.wbuf_size + *offset;
		*offset = protocol->layers.stream.wbuf_offset;
	} else {
		return FAILURE;
	}

	return SUCCESS;
}


struct rpcprotocol_interface *rpcprotocol_stream_new(
	int socket_fd, struct rpcprotocol_info *trans_info) {
	FILE *byte_stream = fdopen(socket_fd, "r+");
	if (byte_stream == NULL)
		return NULL;

	rpcprotocol_private *result = malloc(sizeof *result);
	assert(result);

	*result = (rpcprotocol_private){
		.public_interface = {.errnum = 0,
			.trans_info = trans_info,
			.byte_stream = byte_stream,
			.message_stream = fopencookie(result, "r+",
				(cookie_io_functions_t){.read = &message_stream_read_cookie,
					.write = &message_stream_write_cookie,
					.seek = &message_stream_seek_cookie})},
		.layers = {.stream = {.rmsg_offset = 0,
					   .wbuf = NULL,
					   .wbuf_offset = 0,
					   .wbuf_size = 0,
					   .wbuf_capacity = 0}}};

	int set_buffering_to_none =
		setvbuf(result->public_interface.message_stream, NULL, _IONBF, 0);
	assert(set_buffering_to_none == SUCCESS);

	return (struct rpcprotocol_interface *)result;
}
