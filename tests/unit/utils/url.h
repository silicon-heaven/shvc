#ifndef ITEM_H
#define ITEM_H
#include <shv/cp.h>

#define ck_assert_rpcurl_eq(a, b) \
	do { \
		ck_assert_int_eq((a)->protocol, (b)->protocol); \
		ck_assert_pstr_eq((a)->location, (b)->location); \
		ck_assert_int_eq((a)->port, (b)->port); \
		ck_assert_pstr_eq((a)->login.username, (b)->login.username); \
		ck_assert_pstr_eq((a)->login.password, (b)->login.password); \
		ck_assert_int_eq((a)->login.login_type, (b)->login.login_type); \
		ck_assert_pstr_eq((a)->login.device_id, (b)->login.device_id); \
		ck_assert_pstr_eq( \
			(a)->login.device_mountpoint, (b)->login.device_mountpoint); \
	} while (false)

#endif
