#include <shv/rpcerror.h>

bool rpcerror_unpack(
	cp_unpack_t unpack, struct cpitem *item, rpcerrno_t *errno, char **errmsg) {
	if (errno)
		*errno = RPCMSG_E_NO_ERROR;
	if (errmsg)
		*errmsg = NULL;

	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return false;
	for_cp_unpack_imap(unpack, item, ikey) {
		switch (ikey) {
			case RPCMSG_ERR_KEY_CODE: {
				switch (cp_unpack_type(unpack, item)) {
					case CPITEM_UINT:
						if (errno)
							cpitem_extract_uint(item, *errno);
						break;
					case CPITEM_INT:
						if (errno)
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
	if (item->type != CPITEM_CONTAINER_END)
		return false;
	if (cp_unpack_type(unpack, item) != CPITEM_CONTAINER_END)
		return false;
	return true;
}
