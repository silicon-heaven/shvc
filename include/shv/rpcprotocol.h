/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCPROTOCOL_H
#define SHV_RPCPROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


enum protocol_scheme {
	TCP,
	UNIX,
	TTY
};

enum data_link_layer {
    STREAM,
	SERIAL
};

union additional_protocol_info {
	uint16_t port;
	uint32_t baudrate;
};

struct protocol_destination {
	char *name;
	union additional_protocol_info additional_info;
};

struct rpcprotocol_info {
    enum protocol_scheme protocol_scheme;
	enum data_link_layer data_link;
	struct protocol_destination destination;
};

struct rpcprotocol_interface {
	FILE *byte_stream;
	FILE *message_stream;
	struct rpcprotocol_info *trans_info;
	int errnum;
};

#endif
