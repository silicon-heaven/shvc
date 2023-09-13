#include <shv/rpcmsg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "rpcmsg_access.gperf.h"
#include "shv/cp.h"


const char *rpcmsg_access_str(enum rpcmsg_access acc) {
	switch (acc) {
		case RPCMSG_ACC_BROWSE:
			return "bws";
		case RPCMSG_ACC_READ:
			return "rd";
		case RPCMSG_ACC_WRITE:
			return "wr";
		case RPCMSG_ACC_COMMAND:
			return "cmd";
		case RPCMSG_ACC_CONFIG:
			return "cfg";
		case RPCMSG_ACC_SERVICE:
			return "srv";
		case RPCMSG_ACC_SUPER_SERVICE:
			return "ssrv";
		case RPCMSG_ACC_DEVEL:
			return "dev";
		case RPCMSG_ACC_ADMIN:
			return "su";
		default:
			return "";
	}
}


enum rpcmsg_access rpcmsg_access_extract(const char *str) {
	char *term;
	do {
		term = strchrnul(str, ',');
		const struct gperf_rpcmsg_access_match *match =
			gperf_rpcmsg_access(str, term - str);
		if (match)
			return match->acc;
		str = term + 1;
	} while (*term == ',');
	return RPCMSG_ACC_INVALID;
}

enum rpcmsg_access rpcmsg_access_unpack(cp_unpack_t unpack, struct cpitem *item) {
	FILE *f = cp_unpack_fopen(unpack, item);
	if (item->type != CPITEM_STRING)
		goto error;
	/* The longest access right is four characters. We load five to prevent
	 * match on same prefix and need one more for null byte.
	 */
	char str[6];
	while (true) {
		if (fscanf(f, "%5[^,]", str) != 1)
			goto error;
		const struct gperf_rpcmsg_access_match *match =
			gperf_rpcmsg_access(str, strlen(str));
		if (match) {
			fclose(f);
			if (!(item->as.String.flags & CPBI_F_LAST))
				cp_unpack_skip(unpack, item);
			return match->acc;
		}
		int c;
		do
			c = fgetc(f);
		while (c != EOF && c != ',');
	}
error:
	if (f)
		fclose(f);
	return RPCMSG_ACC_INVALID;
}
