#include <shv/rpcerror.h>


bool rpcerror_unpack(
	cp_unpack_t unpack, struct cpitem *item, rpcerrno_t *errno, char **errmsg) {
	*errno = RPCERR_NO_ERROR;
	if (errmsg)
		*errmsg = NULL;

	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return false;
	for_cp_unpack_imap(unpack, item, ikey) {
		switch (ikey) {
			case RPCMSG_ERR_KEY_CODE: {
				switch (cp_unpack_type(unpack, item)) {
					case CPITEM_UINT:
						cpitem_extract_uint(item, *errno);
						break;
					case CPITEM_INT:
						cpitem_extract_int(item, *errno);
						break;
					default:
						return false;
				}
				break;
			}
			case RPCMSG_ERR_KEY_MESSAGE:
				if (cp_unpack_type(unpack, item) != CPITEM_STRING)
					return false;
				if (errmsg)
					*errmsg = cp_unpack_strdup(unpack, item);
				break;
			default: /* Just ignore any unknown key */
				cp_unpack_skip(unpack, item);
				break;
		}
	}
	return *errno != RPCERR_NO_ERROR;
}

bool rpcerror_pack(cp_pack_t pack, rpcerrno_t errno, const char *errmsg) {
#define G(V) \
	do { \
		if (!(V)) \
			return false; \
	} while (false)

	G(cp_pack_imap_begin(pack));
	G(cp_pack_int(pack, RPCMSG_ERR_KEY_CODE));
	G(cp_pack_int(pack, errno));
	if (errmsg) {
		G(cp_pack_int(pack, RPCMSG_ERR_KEY_MESSAGE));
		G(cp_pack_str(pack, errmsg));
	}
	G(cp_pack_container_end(pack));
	return true;

#undef G
}
