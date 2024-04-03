
#include "shv/rpcprotocol.h"
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>


const uint8_t STX = 0xA2;
const uint8_t ETX = 0xA3;
const uint8_t ATX = 0xA4;
const uint8_t ESC = 0xAA;

const uint8_t ESC_STX = 0x02;
const uint8_t ESC_ETX = 0x03;
const uint8_t ESC_ATX = 0x04;
const uint8_t ESC_ESC = 0x0A;

bool escape(uint8_t byte, uint8_t **escaped_byte_pair) {
	switch (byte) {
	    case STX:
			*escaped_byte_pair = (uint8_t[]){ESC, ESC_STX};
			return true;
		case ETX:
			*escaped_byte_pair = (uint8_t[]){ESC, ESC_ETX};
			return true;
		case ATX:
			*escaped_byte_pair = (uint8_t[]){ESC, ESC_ATX};
			return true;
		case ESC:
			*escaped_byte_pair = (uint8_t[]){ESC, ESC_ESC};
			return true;
		default:
			return false;
	}
}

bool unescape(uint8_t *byte_pair, uint8_t *unescaped_byte) {
	if (byte_pair[0] == ESC) {
		switch (byte_pair[1]) {
			case ESC_STX:
				*unescaped_byte = STX;
				return true;
			case ESC_ETX:
				*unescaped_byte = ETX;
				return true;
			case ESC_ATX:
				*unescaped_byte = ATX;
				return true;
			case ESC_ESC:
				*unescaped_byte = ESC;
				return true;
			default:
				return false;
		}
	} else {
		return false;
	}
}

static ssize_t read_unescaped_bytes_serial(struct rpctransport_common *trans_common, char *buf, size_t size) {
	ssize_t i;
	do
		i = read(trans_common->rfd, buf, size);
	while ((i == -1) && (errno == EINTR));
	if (i == -1)
		trans_common->errnum = errno;
	return i;
}

static int read_single_unescaped_byte_serial(struct rpctransport_common *trans_common) {
	uint8_t v;
	if (read_unescaped_bytes_serial(trans_common, (void *)&v, 1) != 1) {
		trans_common->errnum = ENODATA;
		return -1;
	}
	return v;
}

static bool write_escaped_bytes_serial(struct rpctransport_common *trans_common, const void *buf, size_t size) {
	uint8_t *data = (uint8_t *)buf;
	while (size > 0) {
		ssize_t i = write(trans_common->wfd, data, size);
		if (i == -1) {
			if (errno != EINTR) {
				trans_common->errnum = errno;
				return false;
			}
		} else {
			data += i;
			size -= i;
		}
	}
	return true;
}
