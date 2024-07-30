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

struct cpdatetime clock_cpdatetime(void) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	struct tm tm;
	gmtime_r(&ts.tv_sec, &tm);
	return (struct cpdatetime){
		.msecs = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000),
		.offutc = tm.tm_gmtoff / 60,
	};
}
