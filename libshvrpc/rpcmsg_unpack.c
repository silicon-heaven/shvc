#include <shv/rpcmsg.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


bool rpcmsg_unpack_error(cp_unpack_t unpack, struct cpitem *item,
	enum rpcmsg_error *errnum, char **errmsg) {
	if (errnum)
		*errnum = RPCMSG_E_NO_ERROR;
	if (errmsg)
		*errmsg = NULL;

	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return false;
	for_cp_unpack_imap(unpack, item) {
		switch (item->as.Int) {
			case RPCMSG_ERR_KEY_CODE:
				if (cp_unpack_type(unpack, item) != CPITEM_INT)
					return false;
				if (errnum)
					cpitem_extract_uint(item, *errnum);
				break;
			case RPCMSG_ERR_KEY_MESSAGE:
				if (cp_unpack_type(unpack, item) != CPITEM_STRING)
					return false;
				if (errmsg)
					*errmsg = cp_unpack_strdup(unpack, item);
				break;
			default: /* Just ignore any unknown key */
				break;
		}
	}
	if (item->type != CPITEM_CONTAINER_END)
		return false;
	if (cp_unpack_type(unpack, item) != CPITEM_CONTAINER_END)
		return false;
	return true;
}
