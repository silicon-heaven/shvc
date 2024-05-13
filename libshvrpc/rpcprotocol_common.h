#ifndef SHV_RPCPROTOCOL_COMMON_H
#define SHV_RPCPROTOCOL_COMMON_H

#include <shv/rpcprotocol.h>
#include "crc.h"

#define SINGLE_BYTE 1
#define BYTE_PAIR 2
#define CRC_LEN_IN_BYTES 4

union crc {
	crc_t as_crc_t;
	uint8_t as_bytes[CRC_LEN_IN_BYTES];
};

enum rpcprotocol_errors {
	SUCCESS = 0,
	FAILURE = -1,
};

struct stream_private_vars {
	/* Read */
	size_t rmsg_offset;
	/* Write */
	uint8_t *wbuf;
	size_t wbuf_offset;
	size_t wbuf_size;
	size_t wbuf_capacity;
};

enum state {
	IN_PROGRESS,
	TERMINATED_BY_ETX,
	TERMINATED_BY_ATX,
	TERMINATED_BY_STX
};

struct serial_private_vars {
	bool crc_enabled;
	union crc crc_recv;
	union crc crc_send;
	enum state state_recv;
	enum state state_send;
};

union private_fields {
	struct stream_private_vars stream;
	struct serial_private_vars serial;
};

typedef struct {
	struct rpcprotocol_interface public_interface;
	union private_fields layers;
} rpcprotocol_private;

void set_error(rpcprotocol_private *protocol, int errnum);
void flushmsg(struct rpcprotocol_interface *protocol_interface);
size_t fread_raw_bytes(
	rpcprotocol_private *protocol, char *buf, size_t length, FILE *filestream);
int16_t fread_single_raw_byte(rpcprotocol_private *protocol, FILE *filestream);
bool fwrite_raw_bytes(rpcprotocol_private *protocol, const void *buf,
	size_t size, FILE *filestream);
bool fwrite_single_raw_byte(
	rpcprotocol_private *protocol, uint8_t byte, FILE *filestream);
int message_stream_seek_cookie(void *cookie, off_t *offset, int whence);
void destroy_protocol(struct rpcprotocol_interface *protocol_interface);

#endif
