#include "device_handler.h"

static enum rpchandler_func_res track(struct demo_handler *h, rpcreceive_t receive,
	cp_unpack_t unpack, struct cpitem *item, const struct rpcmsg_meta *meta) {
	return false;
}


static const char *pthcmp(const char *pth, const char *name) {
	size_t len = strlen(name);
	if (!strncmp(pth, name, len) && (pth[len] == '\0' || pth[len] == '/'))
		return &pth[len];
	return NULL;
}

enum rpchandler_func_res demo_device_handler_func(void *ptr, rpcreceive_t receive,
	cp_unpack_t unpack, struct cpitem *item, const struct rpcmsg_meta *meta) {
	struct demo_handler *h = ptr;
	if (meta->path == NULL || *meta->path == '\0') {
		/* We won't be called for anything else because top level is handled by
		 * rpctoplevel.
		 */
		assert(!strcmp(meta->method, "ls"));
	}
	const char *pth1, *pth2;
	if ((pth1 = pthcmp(meta->path, "track"))) {
		if (*pth1 == '\0') {
			return track(h, receive, unpack, item, meta);
		}
	}

	return false;
}
