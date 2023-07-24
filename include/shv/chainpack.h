#ifndef SHV_CHAINPACK_H
#define SHV_CHAINPACK_H

#include <shv/cpcp.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef enum {
	CP_INVALID = -1,

	CP_Null = 128,
	CP_UInt,
	CP_Int,
	CP_Double,
	CP_Bool,
	CP_Blob,
	CP_String,			   // UTF8 encoded string
	CP_DateTimeEpoch_depr, // deprecated
	CP_List,
	CP_Map,
	CP_IMap,
	CP_MetaMap,
	CP_Decimal,
	CP_DateTime,
	CP_CString,

	CP_FALSE = 253,
	CP_TRUE = 254,
	CP_TERM = 255,
} chainpack_pack_packing_schema;

const char *chainpack_packing_schema_name(int sch);

void chainpack_pack_uint_data(cpcp_pack_context *pack_context, uint64_t num);

void chainpack_pack_null(cpcp_pack_context *pack_context) __attribute__((nonnull));

void chainpack_pack_boolean(cpcp_pack_context *pack_context, bool b)
	__attribute__((nonnull));

void chainpack_pack_int(cpcp_pack_context *pack_context, int64_t i)
	__attribute__((nonnull));

void chainpack_pack_uint(cpcp_pack_context *pack_context, uint64_t i)
	__attribute__((nonnull));

void chainpack_pack_double(cpcp_pack_context *pack_context, double d)
	__attribute__((nonnull));

void chainpack_pack_decimal(cpcp_pack_context *pack_context,
	const cpcp_decimal *v) __attribute__((nonnull));

void chainpack_pack_date_time(cpcp_pack_context *pack_context,
	const cpcp_date_time *v) __attribute__((nonnull));

void chainpack_pack_blob(
	cpcp_pack_context *pack_context, const uint8_t *buff, size_t buff_len);
void chainpack_pack_blob_start(cpcp_pack_context *pack_context,
	size_t string_len, const uint8_t *buff, size_t buff_len);
void chainpack_pack_blob_cont(
	cpcp_pack_context *pack_context, const uint8_t *buff, size_t buff_len);

void chainpack_pack_string(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len);
void chainpack_pack_string_start(cpcp_pack_context *pack_context,
	size_t string_len, const char *buff, size_t buff_len);
void chainpack_pack_string_cont(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len);

void chainpack_pack_cstring(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len);
void chainpack_pack_cstring_terminated(
	cpcp_pack_context *pack_context, const char *str);
void chainpack_pack_cstring_start(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len);
void chainpack_pack_cstring_cont(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len);
void chainpack_pack_cstring_finish(cpcp_pack_context *pack_context);

void chainpack_pack_list_begin(cpcp_pack_context *pack_context);
void chainpack_pack_map_begin(cpcp_pack_context *pack_context);
void chainpack_pack_imap_begin(cpcp_pack_context *pack_context);
void chainpack_pack_meta_begin(cpcp_pack_context *pack_context);

void chainpack_pack_container_end(cpcp_pack_context *pack_context);

uint64_t chainpack_unpack_uint_data(cpcp_unpack_context *unpack_context, bool *ok);
uint64_t chainpack_unpack_uint_data2(
	cpcp_unpack_context *unpack_context, int *err_code);
void chainpack_unpack_next(cpcp_unpack_context *unpack_context);

#endif
