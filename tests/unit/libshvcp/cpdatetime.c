#include <shv/cp.h>

#define SUITE "cpdatetime"
#include <check_suite.h>


TEST_CASE(all) {}

static const struct {
	struct cpdatetime dt;
	struct tm tm;
} _dtm[] = {
	{(struct cpdatetime){},
		(struct tm){
			.tm_mday = 1,
			.tm_wday = 4,
			.tm_year = 70,
		}},
	{(struct cpdatetime){.offutc = 60},
		(struct tm){
			.tm_mday = 1,
			.tm_wday = 4,
			.tm_year = 70,
			.tm_gmtoff = 3600,
		}},
};

ARRAY_TEST(all, dttotm, _dtm) {
	struct tm tm = cpdttotm(_d.dt);
	ck_assert_int_eq(tm.tm_sec, _d.tm.tm_sec);
	ck_assert_int_eq(tm.tm_min, _d.tm.tm_min);
	ck_assert_int_eq(tm.tm_hour, _d.tm.tm_hour);
	ck_assert_int_eq(tm.tm_mday, _d.tm.tm_mday);
	ck_assert_int_eq(tm.tm_mon, _d.tm.tm_mon);
	ck_assert_int_eq(tm.tm_year, _d.tm.tm_year);
	ck_assert_int_eq(tm.tm_wday, _d.tm.tm_wday);
	ck_assert_int_eq(tm.tm_yday, _d.tm.tm_yday);
	ck_assert_int_eq(tm.tm_isdst, _d.tm.tm_isdst);
	ck_assert_int_eq(tm.tm_gmtoff, _d.tm.tm_gmtoff);
}
END_TEST

ARRAY_TEST(all, tmtodt, _dtm) {
	struct cpdatetime dt = cptmtodt(_d.tm);
	ck_assert_int_eq(dt.msecs, _d.dt.msecs);
	ck_assert_int_eq(dt.offutc, _d.dt.offutc);
}
END_TEST


static const struct {
	struct cpdatetime dt;
	struct timespec ts;
	bool utc;
} dttots_d[] = {
	{(struct cpdatetime){}, (struct timespec){}, true},
	{(struct cpdatetime){.msecs = 1024},
		(struct timespec){.tv_sec = 1, .tv_nsec = 24000000}, true},
	{(struct cpdatetime){.offutc = 60}, (struct timespec){}, true},
	{(struct cpdatetime){.offutc = 60}, (struct timespec){.tv_sec = 3600}, false},
};
ARRAY_TEST(all, dttots) {
	struct timespec ts = cpdttots(_d.dt, _d.utc);
	ck_assert_int_eq(ts.tv_sec, _d.ts.tv_sec);
	ck_assert_int_eq(ts.tv_nsec, _d.ts.tv_nsec);
}
END_TEST

static const struct {
	struct cpdatetime dt;
	struct timespec ts;
} tstodt_d[] = {
	{(struct cpdatetime){}, (struct timespec){}},
	{(struct cpdatetime){.msecs = 1024},
		(struct timespec){.tv_sec = 1, .tv_nsec = 24000000}},
};
ARRAY_TEST(all, tstodt) {
	struct cpdatetime dt = cptstodt(_d.ts);
	ck_assert_int_eq(dt.msecs, _d.dt.msecs);
	ck_assert_int_eq(dt.offutc, _d.dt.offutc);
}
END_TEST
