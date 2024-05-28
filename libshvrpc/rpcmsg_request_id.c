#include <shv/rpcmsg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

static _Atomic volatile uint8_t request_id = 4;

int rpcmsg_request_id(void) {
	while (true) {
		uint8_t res = atomic_fetch_add(&request_id, 1);
		if (res < 64)
			return res;
		atomic_compare_exchange_weak(&request_id, &res, 4);
	}
}
