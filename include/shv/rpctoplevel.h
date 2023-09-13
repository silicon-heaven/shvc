#ifndef SHV_RPCRESPOND_H
#define SHV_RPCRESPOND_H

#include <stdbool.h>
#include <pthread.h>
#include <shv/rpchandler.h>


typedef struct rpctoplevel *rpctoplevel_t;

void rpctoplevel_destroy(rpctoplevel_t toplevel);

rpctoplevel_t rpctoplevel_new(const char *app_name, const char *app_version,
	bool device) __attribute__((malloc, malloc(rpctoplevel_destroy, 1)));

rpchandler_func *rpctoplevel_func(rpctoplevel_t toplevel)
	__attribute__((nonnull, returns_nonnull));


#endif
