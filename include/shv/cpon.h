#ifndef SHV_CPON_H
#define SHV_CPON_H

#include <shv/cpcp.h>

#include <time.h>

int64_t cpon_timegm(struct tm *tm);
void cpon_gmtime(int64_t epoch_sec, struct tm *tm);

/******************************* P A C K **********************************/

void cpon_pack_null(cpcp_pack_context *pack_context) __attribute__((nonnull));

void cpon_pack_boolean(cpcp_pack_context *pack_context, bool b)
	__attribute__((nonnull));

void cpon_pack_int(cpcp_pack_context *pack_context, int64_t i)
	__attribute__((nonnull));

void cpon_pack_uint(cpcp_pack_context *pack_context, uint64_t i)
	__attribute__((nonnull));

void cpon_pack_double(cpcp_pack_context *pack_context, double d)
	__attribute__((nonnull));

void cpon_pack_decimal(cpcp_pack_context *pack_context, const cpcp_decimal *v)
	__attribute__((nonnull));

void cpon_pack_date_time(cpcp_pack_context *pack_context, const cpcp_date_time *v)
	__attribute__((nonnull));

typedef enum { CCPON_Auto = 0, CCPON_Always, CCPON_Never } cpon_msec_policy;
void cpon_pack_date_time_str(cpcp_pack_context *pack_context, int64_t epoch_msecs,
	int min_from_utc, cpon_msec_policy msec_policy, bool with_tz);

void cpon_pack_blob(
	cpcp_pack_context *pack_context, const uint8_t *s, size_t buff_len);
void cpon_pack_blob_start(
	cpcp_pack_context *pack_context, const uint8_t *buff, size_t buff_len);
void cpon_pack_blob_cont(
	cpcp_pack_context *pack_context, const uint8_t *buff, unsigned buff_len);
void cpon_pack_blob_finish(cpcp_pack_context *pack_context);

void cpon_pack_string(cpcp_pack_context *pack_context, const char *s, size_t l);
void cpon_pack_string_terminated(cpcp_pack_context *pack_context, const char *s);
void cpon_pack_string_start(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len);
void cpon_pack_string_cont(
	cpcp_pack_context *pack_context, const char *buff, unsigned buff_len);
void cpon_pack_string_finish(cpcp_pack_context *pack_context);

void cpon_pack_list_begin(cpcp_pack_context *pack_context);
void cpon_pack_list_end(cpcp_pack_context *pack_context, bool is_oneliner);

void cpon_pack_map_begin(cpcp_pack_context *pack_context);
void cpon_pack_map_end(cpcp_pack_context *pack_context, bool is_oneliner);

void cpon_pack_imap_begin(cpcp_pack_context *pack_context);
void cpon_pack_imap_end(cpcp_pack_context *pack_context, bool is_oneliner);

void cpon_pack_meta_begin(cpcp_pack_context *pack_context);
void cpon_pack_meta_end(cpcp_pack_context *pack_context, bool is_oneliner);

void cpon_pack_copy_str(cpcp_pack_context *pack_context, const char *str);

void cpon_pack_field_delim(
	cpcp_pack_context *pack_context, bool is_first_field, bool is_oneliner);
void cpon_pack_key_val_delim(cpcp_pack_context *pack_context);

/***************************** U N P A C K ********************************/
const char *cpon_unpack_skip_insignificant(cpcp_unpack_context *unpack_context);
void cpon_unpack_next(cpcp_unpack_context *unpack_context);
void cpon_unpack_date_time(cpcp_unpack_context *unpack_context, struct tm *tm,
	int *msec, int *utc_offset);


#endif
