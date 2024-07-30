#include <string.h>
#include <assert.h>
#include <shv/rpcmsg.h>

#include "rpcaccess.gperf.h"


const char *rpcaccess_granted_str(rpcaccess_t acc) {
	switch (acc) {
		case RPCACCESS_BROWSE:
			return "bws";
		case RPCACCESS_READ:
			return "rd";
		case RPCACCESS_WRITE:
			return "wr";
		case RPCACCESS_COMMAND:
			return "cmd";
		case RPCACCESS_CONFIG:
			return "cfg";
		case RPCACCESS_SERVICE:
			return "srv";
		case RPCACCESS_SUPER_SERVICE:
			return "ssrv";
		case RPCACCESS_DEVEL:
			return "dev";
		case RPCACCESS_ADMIN:
			return "su";
		default:
			return "";
	}
}


rpcaccess_t rpcaccess_granted_extract(char *str) {
	rpcaccess_t res = RPCACCESS_NONE;
	while (*str != '\0') {
		char *start = *str == ',' ? str + 1 : str;
		char *term = strchrnul(start, ',');
		const struct gperf_rpcaccess_match *match =
			gperf_rpcaccess(start, term - start);
		if (match) {
			if (match->acc > res)
				res = match->acc;
			/* Discard from the string */
			bool off = str == start && *term != '\0';
			memmove(str, term + (off ? 1 : 0), strlen(term) + (off ? 0 : 1));
		} else
			str = term;
	}
	return res;
}
