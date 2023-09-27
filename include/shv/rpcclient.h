/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_H
#define SHV_RPCCLIENT_H
/*! @file
 * Handle managing a single connection for SHV RPC.
 */

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient_impl.h>

/*! Number of seconds that is default idle time before brokers disconnect
 * clients for inactivity.
 */
#define RPC_DEFAULT_IDLE_TIME 180

/*! Handle used to manage SHV RPC client. */
typedef struct rpcclient *rpcclient_t;


/*! Establish a new connection based on the provided URL.
 *
 * This only estableshes the connection. You might need to perform login if you
 * are connecting to the SHV Broker.
 *
 * @param url: RPC URL with connection info.
 * @returns SHV RPC handle or `NULL` in case connection failed. Based on the
 */
rpcclient_t rpcclient_connect(const struct rpcurl *url);

bool rpcclient_login(rpcclient_t client, const struct rpclogin_options *opts,
	char **errmsg) __attribute__((nonnull(1, 2)));


rpcclient_t rpcclient_stream_new(int readfd, int writefd);
rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port);
rpcclient_t rpcclient_stream_unix_connect(const char *location);

rpcclient_t rpcclient_datagram_new(void);
rpcclient_t rpcclient_datagram_udp_connect(const char *location, int port);

rpcclient_t rpcclient_serial_new(int fd);
rpcclient_t rpcclient_serial_connect(const char *path);


#define rpcclient_destroy(CLIENT) ((CLIENT)->disconnect(CLIENT))

#define rpcclient_connected(CLIENT) ((CLIENT)->rfd != -1)

#define rpcclient_nextmsg(CLIENT) ((CLIENT)->msgfetch(CLIENT, true))

#define rpcclient_validmsg(CLIENT) ((CLIENT)->msgfetch(CLIENT, false))

#define rpcclient_unpack(CLIENT) (&(CLIENT)->unpack)

#define rpcclient_pack(CLIENT) (&(CLIENT)->pack)

#define rpcclient_sendmsg(CLIENT) ((CLIENT)->msgflush(CLIENT, true))

#define rpcclient_dropmsg(CLIENT) ((CLIENT)->msgflush(CLIENT, false))

#define rpcclient_pollfd(CLIENT) ((CLIENT)->rfd)

#define rpcclient_logger(CLIENT) ((CLIENT)->logger)


typedef struct rpcclient_logger *rpcclient_logger_t;

void rpcclient_logger_destroy(rpcclient_logger_t logger);

rpcclient_logger_t rpcclient_logger_new(FILE *f, unsigned maxdepth)
	__attribute__((nonnull, malloc, malloc(rpcclient_logger_destroy)));

/*! Calculate maximum sleep before some message needs to be sent.
 *
 * This time is the number of seconds until the half of the disconnect delay
 * seconds is reached.
 *
 * @param client: Client handle.
 * @param idle_time: Number of seconds before
 * @returns number of seconds before you should send some message.
 */
static inline int rpcclient_maxsleep(rpcclient_t client, int idle_time) {
	struct timespec t;
	assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
	int period = idle_time / 2;
	if (t.tv_sec > client->last_send.tv_sec + period)
		return 0;
	return period - t.tv_sec + client->last_send.tv_sec;
}

#endif
