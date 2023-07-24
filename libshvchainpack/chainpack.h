#ifndef CHAINPACK_H
#define CHAINPACK_H

#include <shv/cp.h>

enum chainpack_scheme {
	CPS_Null = 128,
	CPS_UInt,
	CPS_Int,
	CPS_Double,
	CPS_Bool,
	CPS_Blob,
	CPS_String,				// UTF8 encoded string
	CPS_DateTimeEpoch_depr, // deprecated
	CPS_List,
	CPS_Map,
	CPS_IMap,
	CPS_MetaMap,
	CPS_Decimal,
	CPS_DateTime,
	CPS_CString,

	CPS_FALSE = 253,
	CPS_TRUE = 254,
	CPS_TERM = 255,
};


/* UTC msec since 2.2. 2018 folowed by signed UTC offset in 1/4 hour
 * Fri Feb 02 2018 00:00:00 == 1517529600 EPOCH
 */
static const int64_t chainpack_epoch_msec = 1517529600000;

#endif
