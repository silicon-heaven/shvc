#include <shv/rpcclient_stream.h>
#include <stdlib.h>
#include <unistd.h>

struct ctx {
	const char *location;
	unsigned baudrate;
};


static void tty_connect(void *cookie, int fd[2]) {
	/*struct ctx *c = (struct ctx *)cookie;*/
	// TODO
}

void tty_free(void *cookie) {
	struct ctx *c = (struct ctx *)cookie;
	free(c);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = tty_connect,
	.free = tty_free,
};

rpcclient_t rpcclient_tty_new(
	const char *location, unsigned baudrate, enum rpcstream_proto proto) {
	struct ctx *res = malloc(sizeof *res);
	*res = (struct ctx){
		.location = location,
		.baudrate = baudrate,
	};
	return rpcclient_stream_new(&sclient, res, proto, -1, -1);
}
