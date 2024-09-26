#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <shv/rpctransport.h>


static size_t pipe_peername(void *cookie, int fd[2], char *buf, size_t size) {
	const char *msg = "pipe";
	strncpy(buf, msg, size);
	return strlen(msg);
}

static const struct rpcclient_stream_funcs sclient = {
	.peername = pipe_peername,
	.contrack = true,
};

rpcclient_t rpcclient_pipe_new(int pipes[2], enum rpcstream_proto proto) {
	int rpipe[2];
	if (pipe2(rpipe, 0) == -1)
		return NULL;
	int wpipe[2];
	if (pipe2(wpipe, 0) == -1) {
		close(rpipe[0]);
		close(rpipe[1]);
		return NULL;
	}
	pipes[0] = wpipe[0];
	pipes[1] = rpipe[1];
	// TODO possibly set cloexec on rfpipe[0] and wpipe[1]
	return rpcclient_stream_new(&sclient, NULL, proto, rpipe[0], wpipe[1]);
}
