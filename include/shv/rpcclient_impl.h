#ifndef SHV_RPCCLIENT_IMPL_H
#define SHV_RPCCLIENT_IMPL_H
#include <stdio.h>
#include <stdbool.h>
#include <shv/cp_pack.h>
#include <shv/cp_unpack.h>
#include <semaphore.h>

// TODO possibly hide this and just provide function to allocate it
struct rpcclient_logger {
	FILE *f;
	struct cpon_state cpon_state;
	sem_t semaphore;
};

struct rpcclient {
	/* Receave */
	cp_unpack_func_t unpack;
	bool (*nextmsg)(struct rpcclient *);
	/* Send */
	cp_pack_func_t pack;
	bool (*sendmsg)(struct rpcclient *);
	/* Disconnect */
	void (*disconnect)(struct rpcclient *);
	/* Logging */
	struct rpcclient_logger *logger;
};

void rpcclient_init(struct rpcclient *client) __attribute__((nonnull));

void rpcclient_log_lock(struct rpcclient_logger *, bool in);

void rpcclient_log_item(struct rpcclient_logger *, const struct cpitem *)
	__attribute__((nonnull(2)));

void rpcclient_log_unlock(struct rpcclient_logger *);

#endif
