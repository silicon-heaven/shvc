#include <shv/rpcclient_stream.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

struct ctx {
	const char *location;
};


static void unix_connect(void *cookie, int fd[2]) {
	char *location = cookie;
	fd[0] = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd[0] == -1)
		return;
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, location, sizeof(addr.sun_path) - 1);

	if (connect(fd[0], (const struct sockaddr *)&addr, sizeof(addr)) != -1)
		return;
	close(fd[0]);
	fd[0] = -1;
}

void unix_free(void *cookie) {
	char *location = cookie;
	free(location);
}

static const struct rpcclient_stream_funcs sclient = {
	.connect = unix_connect,
	.free = unix_free,
};

rpcclient_t rpcclient_unix_new(const char *location, enum rpcstream_proto proto) {
	char *l = strdup(location);
	return rpcclient_stream_new(&sclient, l, proto, -1, -1);
}
