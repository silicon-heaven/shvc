#include <shv/cp.h>

static const char *const errmsg[] = {
	[CPERR_NONE] = "No error",
	[CPERR_EOF] = "End of input",
	[CPERR_IO] = "Input error",
	[CPERR_INVALID] = "Invalid data format",
	[CPERR_OVERFLOW] = "Value is too big to be handled by this implementation",
};

const char *cperror_str(enum cperror tp) {
	return errmsg[tp <= CPERR_OVERFLOW ? tp : 0];
}
