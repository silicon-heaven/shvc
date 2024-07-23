#include <shv/rpcclient_stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const struct rpcclient_stream_funcs sclient = {};

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
