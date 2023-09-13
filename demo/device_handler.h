#ifndef _DEVICE_HANDLER_H
#define _DEVICE_HANDLER_H
#include <shv/rpchandler.h>


struct demo_handler {
	rpchandler_func func;
};


enum rpchandler_func_res demo_device_handler_func(void *ptr, rpcreceive_t receive,
	cp_unpack_t unpack, struct cpitem *item, const struct rpcmsg_meta *meta);

#endif
