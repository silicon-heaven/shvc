/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCSERVER_H
#define SHV_RPCSERVER_H
#include <stdbool.h>
#include <shv/rpcurl.h>

typedef struct rpcserver *rpcserver_t;


rpcserver_t rpcserver_listen(const struct rpcurl *url);

rpcserver_t rpcserver_stream_new(int readfd, int writefd);
rpcserver_t rpcserver_stream_tcp_listen(const char *location, int port);
rpcserver_t rpcserver_stream_unix_listen(const char *location);

rpcserver_t rpcserver_datagram_new(void);
rpcserver_t rpcserver_datagram_udp_listen(const char *location, int port);

#endif
