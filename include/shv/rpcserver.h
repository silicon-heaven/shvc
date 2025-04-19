/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCSERVER_H
#define SHV_RPCSERVER_H
#include <stdbool.h>
#include <shv/rpcclient.h>

/**
 * Handle managing a server waiting for connections for SHV RPC.
 */


/** Operations performed by control function :c:var:`rpcserver.ctrl` for RPC
 * Server.
 *
 * This is the same idea as for :c:enum:`rpcclient_ctrlop` just for servers.
 */
enum rpcserver_ctrlop {
	/** :c:macro:`rpcserver_destroy` */
	RPCS_CTRLOP_DESTROY,
	/** :c:macro:`rpcserver_pollfd` */
	RPCS_CTRLOP_POLLFD,
};

/** Public definition of RPC Server object.
 *
 * It provides abstraction on top of multiple different server implementations.
 */
struct rpcserver {
	/** Control function. Do not use directly. */
	int (*ctrl)(struct rpcserver *server, enum rpcserver_ctrlop op);
	/** Accept client function. Do not use directly. */
	rpcclient_t (*accept)(struct rpcserver *server);
};

/** Handle used to manage SHV RPC server. */
typedef struct rpcserver *rpcserver_t;


/** Destroy the RPC server object.
 *
 * :param SERVER: Pointer to the RPC server object.
 */
#define rpcserver_destroy(SERVER) \
	((void)(SERVER)->ctrl(SERVER, RPCS_CTRLOP_DESTROY))

/** Accept the new client.
 *
 * It is not guaranteed if this is blocking or non-blocking call. In general if
 * :c:macro:`rpcserver_pollfd` provides valid file descriptor then this should
 * be most likely blocking, but you should always use polling to detect if there
 * is a client ready to be accepted.
 *
 * :param SERVER: Pointer to the RPC server object.
 * :return: RPC Client object. It can also return ``NULL`` in case client wasn't
 *   accepted by the server.
 */
#define rpcserver_accept(SERVER) ((rpcclient_t)(SERVER)->accept(SERVER))

/** This provides access to the underlying file descriptor used for accepting
 * clients.
 *
 * This should be used only in poll operations. Do not use other operations on
 * it because that can break internal state consistency of the RPC server.
 *
 * :param SERVER: Pointer to the RPC server object.
 * :return: Integer with file descriptor or ``-1`` if server implementation
 *   can't provide it.
 */
#define rpcserver_pollfd(SERVER) \
	((int)(SERVER)->ctrl(SERVER, RPCS_CTRLOP_POLLFD))

#endif
