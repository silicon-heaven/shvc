#include <shv/rpctoplevel.h>
#include <shv/rpcnode.h>
#include <stdlib.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

struct rpctoplevel {
	rpchandler_func func;
	const char *app_name;
	const char *app_version;
	bool device;
};

static enum rpchandler_func_res func(void *ptr, rpcreceive_t receive,
	cp_unpack_t unpack, struct cpitem *item, const struct rpcmsg_meta *meta) {
	struct rpctoplevel *h = ptr;
	if (meta->type != RPCMSG_T_REQUEST ||
		(meta->path != NULL && *meta->path != '\0'))
		return false;
	cp_pack_t pack = rpcreceive_response_new(receive);

	if (!strcmp(meta->method, "appName")) {
		if (!rpcreceive_validmsg(receive))
			return RPCHFR_RECV_ERR;
		cp_pack_str(pack, h->app_name);
		cp_pack_container_end(pack);

	} else if (!strcmp(meta->method, "appVersion")) {
		if (!rpcreceive_validmsg(receive))
			return RPCHFR_RECV_ERR;
		cp_pack_str(pack, h->app_version);
		cp_pack_container_end(pack);

	} else {
		struct obstack obstack;
		obstack_init(&obstack);

		if (!strcmp(meta->method, "echo")) {
			// TODO
			//

			/*} else if (!strcmp(meta->method, "dir")) {*/
			/*struct rpcnode_dir_request req;*/
			/*if (!rpcnode_unpack_dir(unpack, item, &req, &obstack)) {*/
			/*obstack_free(&obstack, NULL);*/
			/*[>return;<]*/
			/*}*/

		} else {
			rpcmsg_pack_ferror(pack, meta, RPCMSG_E_METHOD_NOT_FOUND,
				"No such path '%s' or method '%s' or insufficient access rights.",
				meta->path, meta->method);
		}
	}

	if (!rpcreceive_response_send(receive))
		return RPCHFR_SEND_ERR;
	return RPCHFR_HANDLED;
}

rpctoplevel_t rpctoplevel_new(
	const char *app_name, const char *app_version, bool device) {
	struct rpctoplevel *res = malloc(sizeof *res);
	*res = (struct rpctoplevel){
		.func = func,
		.app_name = app_name,
		.app_version = app_version,
		.device = device,
	};
	return res;
}

void rpctoplevel_destroy(rpctoplevel_t toplevel) {
	free(toplevel);
}

rpchandler_func *rpctoplevel_func(rpctoplevel_t toplevel) {
	return &toplevel->func;
}
