#include <shv/rpcclient.h>
#include <shv/rpcprotocol.h>
#include <shv/chainpack.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include "rpcprotocol_common.h"


static void set_error(rpcprotocol_private *protocol, int errnum) {
	protocol->public_interface.errnum = errnum;
}

static bool write_bytes_stream(rpcprotocol_private *protocol, const void *buf, size_t size) {
	const uint8_t *data = (const uint8_t *)buf;

	while (size > 0) {
		unsigned long bytes_written = fwrite(data, SINGLE_BYTE, size, protocol->public_interface.byte_stream);
		if (ferror(protocol->public_interface.byte_stream)) {
			if (errno != EINTR) {
				set_error(protocol, errno);
				return false;
			}
		} else {
			data += bytes_written;
			size -= bytes_written;
		}
	}

	fflush(protocol->public_interface.byte_stream);

	return true;
}

static bool encode_and_write_message_size(rpcprotocol_private *protocol, size_t len) {
	unsigned num_bytes = chainpack_w_uint_bytes(len);

	for (unsigned byte_index = 0; byte_index < num_bytes; byte_index++) {
		uint8_t value = byte_index == 0 ? chainpack_w_uint_value1(len, num_bytes)
						   : 0xff & (len >> (CHAR_BIT * (num_bytes - byte_index - 1)));
		if (!write_bytes_stream(protocol, &value, SINGLE_BYTE))
			return false;
	}

	return true;
}

static size_t read_bytes_stream(rpcprotocol_private *protocol, char *buf, size_t length) {
	size_t bytes_read = 0;

	do {
		clearerr(protocol->public_interface.byte_stream);
		bytes_read = fread(buf, SINGLE_BYTE, length, protocol->public_interface.byte_stream);
	} while (ferror(protocol->public_interface.byte_stream) && errno == EINTR);

	if (feof(protocol->public_interface.byte_stream))
        return 0;

	if (ferror(protocol->public_interface.byte_stream))
		set_error(protocol, errno);

	return bytes_read;
}

static int16_t read_single_byte_stream(rpcprotocol_private *protocol) {
	uint8_t byte;

	if (read_bytes_stream(protocol, (void *)&byte, SINGLE_BYTE) != SINGLE_BYTE) {
		set_error(protocol, ENODATA);
		return FAILURE;
	}

	return byte;
}

static ssize_t receive_and_decode_message_size(rpcprotocol_private *protocol) {
	int current_byte = read_single_byte_stream(protocol);

	if (is_error(current_byte))
		return FAILURE;

	unsigned num_bytes = chainpack_int_bytes((uint8_t)current_byte);
	size_t result = chainpack_uint_value1((uint8_t)current_byte, num_bytes);

	for (unsigned byte_index = 1; byte_index < num_bytes; byte_index++) {
		current_byte = read_single_byte_stream(protocol);
		if (is_error(current_byte))
			return FAILURE;
		result = (result << CHAR_BIT) | current_byte;
	}

	return (ssize_t)result;
}

static ssize_t message_stream_write_cookie(void *cookie, const char *buf, size_t size) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;

	if (protocol->wbuf == NULL) {
		protocol->wbuf = malloc(size);
		assert(protocol->wbuf);
	}
	if (protocol->wbuf_capacity == 0)
		protocol->wbuf_capacity = size;

	size_t offset_after_write = protocol->wbuf_offset + size;
	size_t growth_factor = offset_after_write / protocol->wbuf_capacity;

	if ((growth_factor > 0) && (protocol->wbuf_capacity < offset_after_write)) {
		uint8_t *new_wbuf;
		size_t capacity_after_growth = protocol->wbuf_capacity * (growth_factor + 1);
		new_wbuf = realloc(protocol->wbuf, capacity_after_growth);

		assert(new_wbuf);

		protocol->wbuf = new_wbuf;
		protocol->wbuf_capacity = capacity_after_growth;
	}

	memcpy(protocol->wbuf + protocol->wbuf_offset, buf, size);
	protocol->wbuf_offset = offset_after_write;
	if (protocol->wbuf_offset > protocol->wbuf_size)
		protocol->wbuf_size = protocol->wbuf_offset;

	return (ssize_t)size;
}

static ssize_t message_stream_read_cookie(void *cookie, char *buf, size_t size) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;

	if (protocol->rmsg_offset < size)
		/* Read up to the end of the message and then signal EOF by reading 0. */
		size = protocol->rmsg_offset;

	ssize_t bytes_read = (ssize_t)read_bytes_stream(protocol, buf, size);
	if (bytes_read > 0)
		protocol->rmsg_offset -= bytes_read;

	return bytes_read;
}

int8_t nextmsg_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	ssize_t msgsiz = receive_and_decode_message_size(protocol);
	if (msgsiz == MESSAGE_LENGTH_NOT_VALID)
		return MESSAGE_LENGTH_NOT_VALID;

	protocol->rmsg_offset = msgsiz;
	clearerr(protocol->public_interface.message_stream);

	return SUCCESS;
}

static int message_stream_seek_cookie(void *cookie, off_t *offset, int whence) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;
	fflush(protocol->public_interface.message_stream);

	if (whence == SEEK_SET) {
		if ((*offset < 0) || (*offset > protocol->wbuf_offset))
			return FAILURE;

		protocol->wbuf_offset = *offset;
		*offset = protocol->wbuf_offset;
	} else if (whence == SEEK_CUR) {
		if ((*offset + protocol->wbuf_offset > protocol->wbuf_size) || (*offset + (ssize_t)protocol->wbuf_offset < 0))
			return FAILURE;

		protocol->wbuf_offset += *offset;
		*offset = protocol->wbuf_offset;
	} else if (whence == SEEK_END) {
		if (*offset > 0)
			return FAILURE;

		protocol->wbuf_offset = protocol->wbuf_size + *offset;
		*offset = protocol->wbuf_offset;
	} else {
		return FAILURE;
	}

	return SUCCESS;
}

bool msgsend_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	fflush(protocol->public_interface.message_stream);

	if (!encode_and_write_message_size(protocol, protocol->wbuf_size + 1))
		return false;

	uint8_t cp_flag = CP_ChainPack;
	if (!write_bytes_stream(protocol, &cp_flag, SINGLE_BYTE) ||
		!write_bytes_stream(protocol, protocol->wbuf, protocol->wbuf_size))
		return false;

	fseek(protocol->public_interface.message_stream, 0, SEEK_SET);

	return true;
}

bool msgflush_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	fflush(protocol->public_interface.message_stream);
	fseek(protocol->public_interface.message_stream, 0, SEEK_SET);

	return true;
}

void destroy_protocol_stream(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	free(protocol->public_interface.trans_info->destination.name);
	free(protocol->public_interface.trans_info);
	fclose(protocol->public_interface.byte_stream);
	fclose(protocol->public_interface.message_stream);
	free(protocol->wbuf);
	free(protocol);
}

struct rpcprotocol_interface *rpcprotocol_stream_new(int socket_fd, struct rpcprotocol_info *trans_info) {
	rpcprotocol_private *result = malloc(sizeof *result);
	assert(result);
	FILE *byte_stream = fdopen(socket_fd, "r+");
	assert(byte_stream);

	result->rmsg_offset = 0;
	result->wbuf = NULL;
	result->wbuf_size = 0;
	result->wbuf_offset = 0;
	result->wbuf_capacity = 0;

	result->public_interface.errnum = 0;
	result->public_interface.trans_info = trans_info;
	result->public_interface.byte_stream = byte_stream;
	result->public_interface.message_stream =
		fopencookie(result, "r+",
			(cookie_io_functions_t){
				.read = &message_stream_read_cookie,
				.write = &message_stream_write_cookie,
				.seek = &message_stream_seek_cookie
			});

	return (struct rpcprotocol_interface *)result;
}


