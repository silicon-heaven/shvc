#ifndef SHV_RPCPROTOCOL_COMMON_H
#define SHV_RPCPROTOCOL_COMMON_H

#include <shv/rpcprotocol.h>

#define SINGLE_BYTE 1

static inline bool is_error(ssize_t value) {
	return value < 0;
}

enum rpcprotocol_errors {
	SUCCESS = 0,
	FAILURE = -1,
	CP_FORMAT_FLAG_NOT_VALID = -2,
	MESSAGE_LENGTH_NOT_VALID = -3,
};

typedef struct {
	struct rpcprotocol_interface public_interface;
	/* Private fields */
	/* Read */
	size_t rmsg_offset;
	/* Write */
	uint8_t *wbuf;
	size_t wbuf_offset;
	size_t wbuf_size;
	size_t wbuf_capacity;
} rpcprotocol_private;

void flushmsg(struct rpcprotocol_interface *protocol_interface);

#endif
