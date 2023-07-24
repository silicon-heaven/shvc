#ifndef SHV_CPCP_H
#define SHV_CPCP_H

#include <shv/cpcp_pack.h>
#include <shv/cpcp_unpack.h>


enum cpcp_format {
	CPCP_ChainPack = 1,
	CPCP_Cpon,
};

enum cpcp_error {
	CPCP_RC_OK = 0,
	CPCP_RC_MALLOC_ERROR,
	CPCP_RC_BUFFER_OVERFLOW,
	CPCP_RC_BUFFER_UNDERFLOW,
	CPCP_RC_MALFORMED_INPUT,
	CPCP_RC_LOGICAL_ERROR,
	CPCP_RC_CONTAINER_STACK_OVERFLOW,
	CPCP_RC_CONTAINER_STACK_UNDERFLOW,
};

const char *cpcp_error_string(enum cpcp_error errno);


#endif
