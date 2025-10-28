#include <shv/rpcerror.h>


bool rpcerror_unpack(cp_unpack_t unpack, struct cpitem *item,
	rpcerrno_t *errnum, char **errmsg) {
	*errnum = RPCERR_NO_ERROR;
	if (errmsg)
		*errmsg = NULL;

	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return false;
	for_cp_unpack_imap(unpack, item, ikey) {
		switch (ikey) {
			case RPCMSG_ERR_KEY_CODE: {
				switch (cp_unpack_type(unpack, item)) {
					case CPITEM_UINT:
						cpitem_extract_uint(item, *errnum);
						break;
					case CPITEM_INT:
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
	return *errnum != RPCERR_NO_ERROR;
}

bool rpcerror_pack(cp_pack_t pack, rpcerrno_t errnum, const char *errmsg) {
#define G(V) \
	do { \
		if (!(V)) \
			return false; \
	} while (false)

	G(cp_pack_imap_begin(pack));
	G(cp_pack_int(pack, RPCMSG_ERR_KEY_CODE));
	G(cp_pack_int(pack, errnum));
	if (errmsg) {
		G(cp_pack_int(pack, RPCMSG_ERR_KEY_MESSAGE));
		G(cp_pack_str(pack, errmsg));
	}
	G(cp_pack_container_end(pack));
	return true;

#undef G
}

static rpcerrno_t sanitize(rpcerrno_t errnum) {
	return errnum >= RPCERR_METHOD_CALL_EXCEPTION_BASE
		? RPCERR_METHOD_CALL_EXCEPTION
		: errnum > RPCERR_NOT_IMPLEMENTED ? RPCERR_UNKNOWN
										  : errnum;
}

const char *rpcerror_str(rpcerrno_t errnum) {
	static const char *const errname[] = {
		[RPCERR_NO_ERROR] = "NoError",
		[1] = "InvalidRequest",
		[RPCERR_METHOD_NOT_FOUND] = "MethodNotFound",
		[RPCERR_INVALID_PARAM] = "InvalidParam",
		[4] = "InternalError",
		[5] = "ParseError",
		[RPCERR_USR1] = "ImplementationReserved1",
		[RPCERR_USR2] = "ImplementationReserved2",
		[RPCERR_METHOD_CALL_EXCEPTION] = "MethodCallException",
		[RPCERR_UNKNOWN] = "Unknown",
		[RPCERR_LOGIN_REQUIRED] = "LoginRequired",
		[RPCERR_USER_ID_REQUIRED] = "UserIDRequired",
		[RPCERR_NOT_IMPLEMENTED] = "NotImplemented",
		[RPCERR_TRY_AGAIN_LATER] = "TryAgainLater",
		[RPCERR_REQUEST_INVALID] = "RequestInvalid",
	};
	return errname[sanitize(errnum)] ?: errname[RPCERR_UNKNOWN];
}

const char *rpcerror_desc(rpcerrno_t errnum) {
	static const char *const errdesc[] = {
		[RPCERR_NO_ERROR] = "No error was encountered",
		[1] = "The received value is not a valid RPC Request object",
		[RPCERR_METHOD_NOT_FOUND] =
			"The requested method does not exist or is not available or "
			"not accessible with given access level",
		[RPCERR_INVALID_PARAM] = "Invalid request parameter",
		[4] = "Invernal RPC error",
		[5] = "Invalid ChainPack was received",
		[RPCERR_USR1] = "Implementation specific error 1",
		[RPCERR_USR2] = "Implementation specific error 2",
		[RPCERR_METHOD_CALL_EXCEPTION] = "Method execution exception",
		[RPCERR_UNKNOWN] = "Unknown error",
		[RPCERR_LOGIN_REQUIRED] =
			"Method call attempted without previous successful login",
		[RPCERR_USER_ID_REQUIRED] = "Method call requires UserID to be present",
		[RPCERR_NOT_IMPLEMENTED] = "Method is valid but not implemented",
		[RPCERR_TRY_AGAIN_LATER] = "Try again later",
		[RPCERR_REQUEST_INVALID] = "Request is not known",
	};
	return errdesc[sanitize(errnum)] ?: errdesc[RPCERR_UNKNOWN];
}
