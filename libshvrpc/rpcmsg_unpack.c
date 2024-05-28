#include <shv/rpcmsg.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


bool rpcmsg_unpack_error(cp_unpack_t unpack, struct cpitem *item,
	rpcmsg_error *errnum, char **errmsg) {
	if (errnum)
		*errnum = RPCMSG_E_NO_ERROR;
	if (errmsg)
		*errmsg = NULL;

	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return false;
	for_cp_unpack_imap(unpack, item, ikey) {
		switch (ikey) {
			case RPCMSG_ERR_KEY_CODE: {
				switch (cp_unpack_type(unpack, item)) {
					case CPITEM_UINT:
						if (errnum) {
							cpitem_extract_uint(item, *errnum);
						}
						break;
					case CPITEM_INT:
						if (errnum)
							cpitem_extract_int(item, *errnum);
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
