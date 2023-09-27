/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_IMPL_H
#define SHV_RPCCLIENT_IMPL_H
/*! @file
 * Implementation of the SHV RPC client handle. The implementation is defined
 * publically to allow possibly anyone to provide custom implementation but at
 * the same time it is split to clearly differentiate that direct access from
 * the users to the handle is not desirable.
 */

#include <stdbool.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>

// TODO possibly hide this and just provide function to allocate it

struct rpcclient {
	int rfd;
	cp_unpack_func_t unpack;
	bool (*msgfetch)(struct rpcclient *, bool newmsg);
	struct timespec last_receive;

	cp_pack_func_t pack;
	bool (*msgflush)(struct rpcclient *, bool send);
	struct timespec last_send;

	void (*disconnect)(struct rpcclient *);

	struct rpcclient_logger *logger;
};

void rpcclient_init(struct rpcclient *client) __attribute__((nonnull));

void rpcclient_log_lock(struct rpcclient_logger *, bool in);

void rpcclient_log_item(struct rpcclient_logger *, const struct cpitem *)
	__attribute__((nonnull(2)));

void rpcclient_log_unlock(struct rpcclient_logger *);

static inline void rpcclient_last_receive_update(struct rpcclient *client) {
	clock_gettime(CLOCK_MONOTONIC, &client->last_receive);
}

static inline void rpcclient_last_send_update(struct rpcclient *client) {
	clock_gettime(CLOCK_MONOTONIC, &client->last_send);
}

#endif
