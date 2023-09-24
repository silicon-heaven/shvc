#ifndef _DEVICE_HANDLER_H
#define _DEVICE_HANDLER_H
#include <shv/rpchandler.h>


struct device_state {
	struct track {
		int *values;
		size_t cnt;
	} tracks[9];
};

extern const struct rpchandler_funcs device_handler_funcs;

struct device_state *device_state_new(void);

void device_state_free(struct device_state *state);

#endif
