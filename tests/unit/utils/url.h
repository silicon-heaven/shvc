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
		switch ((a)->protocol) { \
			case RPC_PROTOCOL_TTY: \
				ck_assert_int_eq((a)->tty.baudrate, (b)->tty.baudrate); \
				break; \
			case RPC_PROTOCOL_CAN: \
				ck_assert_int_eq((a)->can.local_address, (b)->can.local_address); \
				break; \
			case RPC_PROTOCOL_SSL: \
			case RPC_PROTOCOL_SSLS: \
				ck_assert_pstr_eq((a)->ssl.ca, (b)->ssl.ca); \
				ck_assert_pstr_eq((a)->ssl.cert, (b)->ssl.cert); \
				ck_assert_pstr_eq((a)->ssl.key, (b)->ssl.key); \
				ck_assert_pstr_eq((a)->ssl.crl, (b)->ssl.crl); \
				ck_assert((a)->ssl.noverify == (b)->ssl.noverify); \
				break; \
			default: \
				break; \
		} \
	} while (false)

#endif
