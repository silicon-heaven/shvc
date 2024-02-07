#include "time.h"
#include <shv/cp.h>

struct tm cpdttotm(struct cpdatetime v) {
	struct tm res;
	time_t t = v.msecs / 1000;
	gmtime_r(&t, &res);
	res.tm_gmtoff = v.offutc * 60;
	return res;
}

struct cpdatetime cptmtodt(struct tm v) {
	int32_t utcoff = v.tm_gmtoff;
	return (struct cpdatetime){
		.msecs = timegm(&v) * 1000,
		.offutc = utcoff / 60,
	};
}
