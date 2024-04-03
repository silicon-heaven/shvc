#include <stdio.h>
#include <stdlib.h>
#include <shv/rpcprotocol.h>
#include <assert.h>
#include "rpcprotocol_common.h"


void flushmsg(struct rpcprotocol_interface *protocol_interface) {
	/* Flush the rest of the message. Read up to the EOF. */
	char buf[BUFSIZ];
	while (fread(buf, 1, BUFSIZ, protocol_interface->message_stream) > 0) {}
}

