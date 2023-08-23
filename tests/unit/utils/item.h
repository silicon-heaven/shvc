#ifndef ITEM_H
#define ITEM_H
#include <shv/cp.h>


#define ck_assert_blob(a, b) \
	do { \
		ck_assert_int_eq(a.as.Blob.len, b.as.Blob.len); \
		ck_assert_mem_eq(a.buf, b.buf, a.as.Blob.len); \
		ck_assert_int_eq(a.as.Blob.eoff, b.as.Blob.eoff); \
		ck_assert_uint_eq(a.as.Blob.flags, b.as.Blob.flags); \
	} while (false)

#define ck_assert_item(a, b) \
	do { \
		ck_assert_int_eq(a.type, b.type); \
		switch (a.type) { \
			case CP_ITEM_INVALID: \
			case CP_ITEM_NULL: \
				break; \
			case CP_ITEM_BOOL: \
				ck_assert(a.as.Bool == b.as.Bool); \
				break; \
			case CP_ITEM_INT: \
				ck_assert_int_eq(a.as.Int, b.as.Int); \
				break; \
			case CP_ITEM_UINT: \
				ck_assert_uint_eq(a.as.UInt, b.as.UInt); \
				break; \
			case CP_ITEM_DOUBLE: \
				ck_assert_double_eq(a.as.Double, b.as.Double); \
				break; \
			case CP_ITEM_DECIMAL: \
				ck_assert_int_eq(a.as.Decimal.mantisa, b.as.Decimal.mantisa); \
				ck_assert_int_eq(a.as.Decimal.exponent, b.as.Decimal.exponent); \
				break; \
			case CP_ITEM_BLOB: \
			case CP_ITEM_STRING: \
				ck_assert_blob(a, b); \
				break; \
			case CP_ITEM_DATETIME: \
				ck_assert_int_eq(a.as.Datetime.msecs, b.as.Datetime.msecs); \
				ck_assert_int_eq(a.as.Datetime.offutc, b.as.Datetime.offutc); \
				break; \
			case CP_ITEM_LIST: \
			case CP_ITEM_MAP: \
			case CP_ITEM_IMAP: \
			case CP_ITEM_META: \
			case CP_ITEM_CONTAINER_END: \
				break; \
			default: \
				abort(); \
		}; \
	} while (false)


#define ck_assert_item_type(i, tp) ck_assert_int_eq(i.type, tp)

#define ck_assert_item_bool(i, v) \
	do { \
		ck_assert_int_eq(i.type, CP_ITEM_BOOL); \
		ck_assert_int_eq(i.as.Bool, v); \
	} while (false)
#define ck_assert_item_true(i) ck_assert_item_bool(i, true)
#define ck_assert_item_false(i) ck_assert_item_bool(i, false)

#define ck_assert_item_int(i, v) \
	do { \
		ck_assert_int_eq(i.type, CP_ITEM_INT); \
		ck_assert_int_eq(i.as.Int, v); \
	} while (false)

#define ck_assert_item_uint(i, v) \
	do { \
		ck_assert_int_eq(i.type, CP_ITEM_UINT); \
		ck_assert_uint_eq(i.as.Int, v); \
	} while (false)

#define ck_assert_item_double(i, v) \
	do { \
		ck_assert_int_eq(i.type, CP_ITEM_DOUBLE); \
		ck_assert_double_eq(i.as.Double, v); \
	} while (false)

#define ck_assert_item_decimal(i, v) \
	do { \
		ck_assert_int_eq(i.type, CP_ITEM_DECIMAL); \
		ck_assert_int_eq(i.as.Decimal.mantisa, v.mantisa); \
		ck_assert_int_eq(i.as.Decimal.exponent, v.exponent); \
	} while (false)

#endif
