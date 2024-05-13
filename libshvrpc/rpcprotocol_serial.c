#include <shv/rpcprotocol.h>
#include "rpcprotocol_common.h"
#include "rpcprotocol_serial.h"
#include "crc.h"
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>

#define STX (uint8_t)0xA2
#define ETX (uint8_t)0xA3
#define ATX (uint8_t)0xA4
#define ESC (uint8_t)0xAA

#define ESC_STX (uint8_t)0x02
#define ESC_ETX (uint8_t)0x03
#define ESC_ATX (uint8_t)0x04
#define ESC_ESC (uint8_t)0x0A


static uint8_t escape_byte(uint8_t byte) {
	switch (byte) {
		case STX:
			return ESC_STX;
		case ETX:
			return ESC_ETX;
		case ATX:
			return ESC_ATX;
		case ESC:
			return ESC_ESC;
		default:
			return byte;
	}
}

static uint8_t unescape_byte(const uint8_t byte) {
	switch (byte) {
		case ESC_STX:
			return STX;
		case ESC_ETX:
			return ETX;
		case ESC_ATX:
			return ATX;
		case ESC_ESC:
			return ESC;
		default:
			return byte;
	}
}

static void finalize_crc_recv(rpcprotocol_private *protocol) {
	protocol->layers.serial.crc_recv.as_crc_t =
		crc_finalize(protocol->layers.serial.crc_recv.as_crc_t);
}

static void finalize_crc_send(rpcprotocol_private *protocol) {
	protocol->layers.serial.crc_send.as_crc_t =
		crc_finalize(protocol->layers.serial.crc_send.as_crc_t);
}

static bool msg_recv_ended(const rpcprotocol_private *protocol) {
	return (protocol->layers.serial.state_recv == TERMINATED_BY_ETX);
}

static bool msg_recv_ended_or_aborted(const rpcprotocol_private *protocol) {
	return (msg_recv_ended(protocol) ||
		(protocol->layers.serial.state_recv == TERMINATED_BY_ATX));
}

static bool msg_recv_started_unexpectedly(const rpcprotocol_private *protocol) {
	return (protocol->layers.serial.state_recv == TERMINATED_BY_STX);
}

ssize_t fread_unescaped_bytes(
	rpcprotocol_private *protocol, uint8_t *buf, size_t length) {
	size_t actual_bytes_read = 0;
	bool escape_encountered = false;

	while (actual_bytes_read != length) {
		int16_t read_byte = fread_single_raw_byte(
			protocol, protocol->public_interface.byte_stream);
		if (read_byte == FAILURE)
			return FAILURE;

		uint8_t output_byte;
		if (escape_encountered) {
			output_byte = unescape_byte((uint8_t)read_byte);
		} else {
			if (read_byte == ESC) {
				escape_encountered = true;
				continue;
			} else {
				output_byte = (uint8_t)read_byte;
			}
		}

		buf[actual_bytes_read] = output_byte;
		actual_bytes_read++;
	}

	return (ssize_t)actual_bytes_read;
}

bool validmsg_protocol_serial(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	if (msg_recv_ended(protocol)) {
		if (protocol->layers.serial.crc_enabled) {
			union crc crc_read = {.as_crc_t = crc_init()};
			finalize_crc_recv(protocol);
			fread_unescaped_bytes(protocol, crc_read.as_bytes, CRC_LEN_IN_BYTES);
			return (crc_read.as_crc_t == protocol->layers.serial.crc_recv.as_crc_t);
		} else {
			return true;
		}
	} else
		return false;
}

bool nextmsg_protocol_serial(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;
	clearerr(protocol->public_interface.message_stream);

	if (msg_recv_ended_or_aborted(protocol)) {
		int16_t read_byte = fread_single_raw_byte(
			protocol, protocol->public_interface.byte_stream);
		conditional_crc_recv_init(protocol);
		return (read_byte == (int16_t)STX);
	} else if (msg_recv_started_unexpectedly(protocol)) {
		conditional_crc_recv_init(protocol);
		return true;
	} else {
		return false;
	}
}

bool msgsend_protocol_serial(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	bool write_success = fwrite_single_raw_byte(
		protocol, ETX, protocol->public_interface.byte_stream);
	if (!write_success)
		return false;
	protocol->layers.serial.state_send = TERMINATED_BY_ETX;

	if (protocol->layers.serial.crc_enabled) {
		finalize_crc_send(protocol);
		write_success = fwrite_raw_bytes(protocol,
			protocol->layers.serial.crc_send.as_bytes, CRC_LEN_IN_BYTES,
			protocol->public_interface.byte_stream);
		if (!write_success)
			return false;

		protocol->layers.serial.crc_send.as_crc_t = crc_init();
		return true;
	}

	return true;
}

bool is_sending_msg_in_progress(struct rpcprotocol_interface *protocol_interface) {
	const rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	return (protocol->layers.serial.state_send == IN_PROGRESS);
}

static ssize_t message_serial_write_cookie(
	void *cookie, const char *buf, size_t size) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;

	const uint8_t *l_buf = (const uint8_t *)buf;
	size_t buffer_index = 0;
	bool write_success;

	if (protocol->layers.serial.state_send != IN_PROGRESS) {
		write_success = fwrite_single_raw_byte(
			protocol, STX, protocol->public_interface.byte_stream);
		if (!write_success)
			return 0;

		protocol->layers.serial.state_send = IN_PROGRESS;
	}

	while (buffer_index != size) {
		uint8_t potentially_escaped_byte = escape_byte(l_buf[buffer_index]);
		if (potentially_escaped_byte == l_buf[buffer_index]) {
			write_success = fwrite_single_raw_byte(protocol,
				l_buf[buffer_index], protocol->public_interface.byte_stream);
			if (!write_success)
				return 0;

			conditional_crc_send_update(
				protocol, &l_buf[buffer_index], SINGLE_BYTE);
		} else {
			const uint8_t escaped_byte_pair[2] = {ESC, potentially_escaped_byte};
			write_success = fwrite_raw_bytes(protocol, escaped_byte_pair,
				BYTE_PAIR, protocol->public_interface.byte_stream);
			if (!write_success)
				return 0;

			conditional_crc_send_update(protocol, escaped_byte_pair, BYTE_PAIR);
		}

		buffer_index++;
	}

	return (ssize_t)buffer_index;
}

static ssize_t handle_esc_case(
	rpcprotocol_private *protocol, uint8_t *l_buf, size_t *num_of_read_bytes) {
	conditional_crc_recv_update(protocol, ESC);
	int16_t current_byte =
		fread_single_raw_byte(protocol, protocol->public_interface.byte_stream);
	if (current_byte == FAILURE)
		return FAILURE;

	uint8_t unescaped_byte = unescape_byte((uint8_t)current_byte);
	if (unescaped_byte == (uint8_t)current_byte)
		return FAILURE;

	conditional_crc_recv_update(protocol, (uint8_t)current_byte);
	l_buf[(*num_of_read_bytes)++] = (uint8_t)current_byte;
	protocol->layers.serial.state_recv = IN_PROGRESS;

	return SUCCESS;
}

static ssize_t message_serial_read_cookie(void *cookie, char *buf, size_t size) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)cookie;
	uint8_t *l_buf = (uint8_t *)buf;
	size_t num_of_read_bytes = 0;

	while (num_of_read_bytes != size) {
		int16_t current_byte = fread_single_raw_byte(
			protocol, protocol->public_interface.byte_stream);

		switch (current_byte) {
			case FAILURE:
				return FAILURE;
			case ATX:
				protocol->layers.serial.state_recv = TERMINATED_BY_ATX;
				return FAILURE;
			case STX:
				protocol->layers.serial.state_recv = TERMINATED_BY_STX;
				return FAILURE;
			case ETX:
				protocol->layers.serial.state_recv = TERMINATED_BY_ETX;
				return (ssize_t)num_of_read_bytes;
			case ESC:
				if (handle_esc_case(protocol, l_buf, &num_of_read_bytes) == FAILURE)
					return FAILURE;
				break;
			default:
				conditional_crc_recv_update(protocol, (uint8_t)current_byte);
				l_buf[num_of_read_bytes++] = (uint8_t)current_byte;
		}
	}

	return (ssize_t)num_of_read_bytes;
}

bool abort_message(struct rpcprotocol_interface *protocol_interface) {
	rpcprotocol_private *protocol = (rpcprotocol_private *)protocol_interface;

	return fwrite_single_raw_byte(
		protocol, ATX, protocol->public_interface.byte_stream);
}

struct rpcprotocol_interface *rpcprotocol_serial_new(
	int fd, struct rpcprotocol_info *trans_info, bool crc_enabled) {
	rpcprotocol_private *result = malloc(sizeof *result);
	assert(result);

	FILE *byte_stream = fdopen(fd, "r+");
	assert(byte_stream);

	*result =
		(rpcprotocol_private){.public_interface = {.errnum = 0,
								  .trans_info = trans_info,
								  .byte_stream = byte_stream,
								  .message_stream = fopencookie(result, "r+",
									  (cookie_io_functions_t){
										  .read = &message_serial_read_cookie,
										  .write = &message_serial_write_cookie,
									  })},
			.layers = {.serial = {.crc_enabled = crc_enabled,
						   .crc_send.as_crc_t = crc_init(),
						   .crc_recv.as_crc_t = crc_init(),
						   .state_send = TERMINATED_BY_ETX,
						   .state_recv = TERMINATED_BY_ETX}}};

	int set_buffering_to_none =
		setvbuf(result->public_interface.message_stream, NULL, _IONBF, 0);
	assert(set_buffering_to_none == SUCCESS);

	return (struct rpcprotocol_interface *)result;
}
