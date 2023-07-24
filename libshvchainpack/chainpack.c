#include <shv/chainpack.h>

#include <string.h>
#include <limits.h>


// UTC msec since 2.2. 2018 folowed by signed UTC offset in 1/4 hour
// Fri Feb 02 2018 00:00:00 == 1517529600 EPOCH
static const int64_t SHV_EPOCH_MSEC = 1517529600000;

const char *chainpack_packing_schema_name(int sch) {
	switch (sch) {
		case CP_INVALID:
			return "INVALID";
		case CP_FALSE:
			return "FALSE";
		case CP_TRUE:
			return "TRUE";

		case CP_Null:
			return "Null";
		case CP_UInt:
			return "UInt";
		case CP_Int:
			return "Int";
		case CP_Double:
			return "Double";
		case CP_Bool:
			return "Bool";
		case CP_Blob:
			return "Blob";
		case CP_String:
			return "String";
		case CP_CString:
			return "CString";
		case CP_List:
			return "List";
		case CP_Map:
			return "Map";
		case CP_IMap:
			return "IMap";
		case CP_DateTimeEpoch_depr:
			return "DateTimeEpoch_depr";
		case CP_DateTime:
			return "DateTime";
		case CP_MetaMap:
			return "MetaMap";
		case CP_Decimal:
			return "Decimal";

		case CP_TERM:
			return "TERM";
	}
	return "";
}

static void copy_bytes_cstring(
	cpcp_pack_context *pack_context, const void *str, size_t len) {
	size_t i;
	for (i = 0; i < len; ++i) {
		if (pack_context->err_no != CPCP_RC_OK)
			return;
		uint8_t ch = ((const uint8_t *)str)[i];
		switch (ch) {
			case '\0':
			case '\\':
				cpcp_pack_copy_byte(pack_context, '\\');
				cpcp_pack_copy_byte(pack_context, ch);
				break;
			default:
				cpcp_pack_copy_byte(pack_context, ch);
		}
	}
}

#if defined(__GNUC__) && __GNUC__ >= 4

static int significant_bits_part_length(uint64_t n) {
	int len = 0;
	int llbits = sizeof(long long) * CHAR_BIT;

	if (n == 0)
		return 0;

	if ((llbits < 64) && (n & 0xFFFFFFFF00000000)) {
		len += 32;
		n >>= 32;
	}

	len += llbits - __builtin_clzll(n);

	return len;
}

#else /* Fallback for generic compiler */

// see https://en.wikipedia.org/wiki/Find_first_set#CLZ
static const uint8_t sig_table_4bit[16] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

static int significant_bits_part_length(uint64_t n) {
	int len = 0;

	if (n & 0xFFFFFFFF00000000) {
		len += 32;
		n >>= 32;
	}
	if (n & 0xFFFF0000) {
		len += 16;
		n >>= 16;
	}
	if (n & 0xFF00) {
		len += 8;
		n >>= 8;
	}
	if (n & 0xF0) {
		len += 4;
		n >>= 4;
	}
	len += sig_table_4bit[n];
	return len;
}

#endif /* end of significant_bits_part_length function */

// number of bytes needed to encode bit_len
static int bytes_needed(int bit_len) {
	int cnt;
	if (bit_len <= 28)
		cnt = (bit_len - 1) / 7 + 1;
	else
		cnt = (bit_len - 1) / 8 + 2;
	return cnt;
}

/* UInt
 0 ...  7 bits  1  byte  |0|x|x|x|x|x|x|x|<-- LSB
 8 ... 14 bits  2  bytes |1|0|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB
15 ... 21 bits  3  bytes |1|1|0|x|x|x|x|x| |x|x|x|x|x|x|x|x|
|x|x|x|x|x|x|x|x|<-- LSB 22 ... 28 bits  4  bytes |1|1|1|0|x|x|x|x|
|x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB 29+       bits  5+
bytes |1|1|1|1|n|n|n|n| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|
... <-- LSB n ==  0 ->  4 bytes number (32 bit number) n ==  1 ->  5 bytes
number n == 14 -> 18 bytes number n == 15 -> for future (number of bytes will be
specified in next byte)
*/

static void pack_uint_data_helper(
	cpcp_pack_context *pack_context, uint64_t num, int bit_len) {
	int byte_cnt = bytes_needed(bit_len);
	uint8_t bytes[byte_cnt];
	int i;
	for (i = byte_cnt - 1; i >= 0; --i) {
		uint8_t r = num & 255;
		bytes[i] = r;
		num = num >> 8;
	}

	uint8_t *head = bytes;
	if (bit_len <= 28) {
		uint8_t mask = (uint8_t)(0xf0 << (4 - byte_cnt));
		*head = (uint8_t)(*head & ~mask);
		mask = (uint8_t)(mask << 1);
		*head = *head | mask;
	} else {
		*head = (uint8_t)(0xf0 | (byte_cnt - 5));
	}

	for (i = 0; i < byte_cnt; ++i) {
		uint8_t r = bytes[i];
		cpcp_pack_copy_byte(pack_context, r);
	}
}

void chainpack_pack_uint_data(cpcp_pack_context *pack_context, uint64_t num) {
	const size_t UINT_BYTES_MAX = 18;
	if (sizeof(num) > UINT_BYTES_MAX) {
		pack_context->err_no = CPCP_RC_LOGICAL_ERROR; //("writeData_UInt: value
													  // too big to pack!");
		return;
	}

	int bitlen = significant_bits_part_length(num);
	pack_uint_data_helper(pack_context, num, bitlen);
}

/*
 0 ...  7 bits  1  byte  |0|s|x|x|x|x|x|x|<-- LSB
 8 ... 14 bits  2  bytes |1|0|s|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB
15 ... 21 bits  3  bytes |1|1|0|s|x|x|x|x| |x|x|x|x|x|x|x|x|
|x|x|x|x|x|x|x|x|<-- LSB 22 ... 28 bits  4  bytes |1|1|1|0|s|x|x|x|
|x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|<-- LSB 29+       bits  5+
bytes |1|1|1|1|n|n|n|n| |s|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x| |x|x|x|x|x|x|x|x|
... <-- LSB n ==  0 ->  4 bytes number (32 bit number) n ==  1 ->  5 bytes
number n == 14 -> 18 bytes number n == 15 -> for future (number of bytes will be
specified in next byte)
*/

// return max bit length >= bit_len, which can be encoded by same number of bytes
static int expand_bit_len(int bit_len) {
	int ret;
	int byte_cnt = bytes_needed(bit_len);
	if (bit_len <= 28) {
		ret = byte_cnt * (8 - 1) - 1;
	} else {
		ret = (byte_cnt - 1) * 8 - 1;
	}
	return ret;
}

static void chainpack_pack_int_data(cpcp_pack_context *pack_context, int64_t snum) {
	uint64_t num = (uint64_t)(snum < 0 ? -snum : snum);
	bool neg = (snum < 0);

	int bitlen = significant_bits_part_length(num);
	bitlen++; // add sign bit
	if (neg) {
		int sign_pos = expand_bit_len(bitlen);
		uint64_t sign_bit_mask = (uint64_t)1 << sign_pos;
		num |= sign_bit_mask;
	}
	pack_uint_data_helper(pack_context, num, bitlen);
}

void chainpack_pack_uint(cpcp_pack_context *pack_context, uint64_t i) {
	if (pack_context->err_no)
		return;
	if (i < 64) {
		cpcp_pack_copy_byte(pack_context, i % 64);
	} else {
		cpcp_pack_copy_byte(pack_context, CP_UInt);
		chainpack_pack_uint_data(pack_context, i);
	}
}

void chainpack_pack_int(cpcp_pack_context *pack_context, int64_t i) {
	if (pack_context->err_no)
		return;
	if (i >= 0 && i < 64) {
		cpcp_pack_copy_byte(pack_context, (uint8_t)((i % 64) + 64));
	} else {
		cpcp_pack_copy_byte(pack_context, CP_Int);
		chainpack_pack_int_data(pack_context, i);
	}
}

void chainpack_pack_decimal(cpcp_pack_context *pack_context, const cpcp_decimal *v) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_Decimal);
	chainpack_pack_int_data(pack_context, v->mantisa);
	chainpack_pack_int_data(pack_context, v->exponent);
}

void chainpack_pack_double(cpcp_pack_context *pack_context, double d) {
	if (pack_context->err_no)
		return;

	cpcp_pack_copy_byte(pack_context, CP_Double);

	const uint8_t *bytes = (const uint8_t *)&d;
	int len = sizeof(double);

	int n = 1;
	int i;
	if (*(char *)&n == 1) {
		// little endian if true
		for (i = 0; i < len; i++)
			cpcp_pack_copy_byte(pack_context, bytes[i]);
	} else {
		for (i = len - 1; i >= 0; i--)
			cpcp_pack_copy_byte(pack_context, bytes[i]);
	}
}

void chainpack_pack_date_time(
	cpcp_pack_context *pack_context, const cpcp_date_time *v) {
	if (pack_context->err_no)
		return;

	cpcp_pack_copy_byte(pack_context, CP_DateTime);

	// some arguable optimizations when msec == 0 or TZ_offset == 0
	// this can save byte in packed date-time, but packing scheme is more
	// complicated
	int64_t msecs = v->msecs_since_epoch - SHV_EPOCH_MSEC;
	int offset = (v->minutes_from_utc / 15) & 0x7F;
	int ms = (int)(msecs % 1000);
	if (ms == 0)
		msecs /= 1000;
	if (offset != 0) {
		msecs *= 128;
		msecs |= offset;
	}
	msecs *= 4;
	if (offset != 0)
		msecs |= 1;
	if (ms == 0)
		msecs |= 2;
	chainpack_pack_int_data(pack_context, msecs);
}

void chainpack_pack_null(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;

	cpcp_pack_copy_byte(pack_context, CP_Null);
}

static void chainpack_pack_true(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_TRUE);
}

static void chainpack_pack_false(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_FALSE);
}

void chainpack_pack_boolean(cpcp_pack_context *pack_context, bool b) {
	if (pack_context->err_no)
		return;
	if (b)
		chainpack_pack_true(pack_context);
	else
		chainpack_pack_false(pack_context);
}

void chainpack_pack_list_begin(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_List);
}

void chainpack_pack_map_begin(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_Map);
}

void chainpack_pack_imap_begin(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_IMap);
}

void chainpack_pack_meta_begin(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_MetaMap);
}

void chainpack_pack_container_end(cpcp_pack_context *pack_context) {
	if (pack_context->err_no)
		return;
	cpcp_pack_copy_byte(pack_context, CP_TERM);
}

void chainpack_pack_blob(
	cpcp_pack_context *pack_context, const uint8_t *buff, size_t buff_len) {
	chainpack_pack_blob_start(pack_context, buff_len, buff, buff_len);
}

void chainpack_pack_blob_start(cpcp_pack_context *pack_context,
	size_t string_len, const uint8_t *buff, size_t buff_len) {
	cpcp_pack_copy_byte(pack_context, CP_Blob);
	chainpack_pack_uint_data(pack_context, string_len);
	cpcp_pack_copy_bytes(pack_context, buff, buff_len);
}

void chainpack_pack_blob_cont(
	cpcp_pack_context *pack_context, const uint8_t *buff, size_t buff_len) {
	cpcp_pack_copy_bytes(pack_context, buff, buff_len);
}

void chainpack_pack_string(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len) {
	chainpack_pack_string_start(pack_context, buff_len, buff, buff_len);
}

void chainpack_pack_string_start(cpcp_pack_context *pack_context,
	size_t string_len, const char *buff, size_t buff_len) {
	cpcp_pack_copy_byte(pack_context, CP_String);
	chainpack_pack_uint_data(pack_context, string_len);
	cpcp_pack_copy_bytes(pack_context, buff, buff_len);
}

void chainpack_pack_string_cont(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len) {
	cpcp_pack_copy_bytes(pack_context, buff, buff_len);
}

void chainpack_pack_cstring(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len) {
	chainpack_pack_cstring_start(pack_context, buff, buff_len);
	chainpack_pack_cstring_finish(pack_context);
}

void chainpack_pack_cstring_terminated(
	cpcp_pack_context *pack_context, const char *str) {
	size_t len = strlen(str);
	chainpack_pack_cstring(pack_context, str, len);
}

void chainpack_pack_cstring_start(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len) {
	cpcp_pack_copy_byte(pack_context, CP_CString);
	copy_bytes_cstring(pack_context, buff, buff_len);
}

void chainpack_pack_cstring_cont(
	cpcp_pack_context *pack_context, const char *buff, size_t buff_len) {
	copy_bytes_cstring(pack_context, buff, buff_len);
}

void chainpack_pack_cstring_finish(cpcp_pack_context *pack_context) {
	cpcp_pack_copy_byte(pack_context, '\0');
}

//============================   U N P A C K   =================================

/// @pbitlen is used to enable same function usage for signed int unpacking
static void unpack_uint(cpcp_unpack_context *unpack_context, int *pbitlen) {
	if (pbitlen)
		*pbitlen = 0;
	uint64_t num = 0;
	int bitlen = 0;

	const char *p;
	UNPACK_TAKE_BYTE(p);
	uint8_t head = (uint8_t)(*p);

	int bytes_to_read_cnt;
	if ((head & 128) == 0) {
		bytes_to_read_cnt = 0;
		num = head & 127;
		bitlen = 7;
	} else if ((head & 64) == 0) {
		bytes_to_read_cnt = 1;
		num = head & 63;
		bitlen = 6 + 8;
	} else if ((head & 32) == 0) {
		bytes_to_read_cnt = 2;
		num = head & 31;
		bitlen = 5 + 2 * 8;
	} else if ((head & 16) == 0) {
		bytes_to_read_cnt = 3;
		num = head & 15;
		bitlen = 4 + 3 * 8;
	} else {
		bytes_to_read_cnt = (head & 0xf) + 4;
		bitlen = bytes_to_read_cnt * 8;
	}
	int i;
	for (i = 0; i < bytes_to_read_cnt; ++i) {
		UNPACK_TAKE_BYTE(p);
		uint8_t r = (uint8_t)(*p);
		num = (num << 8) + r;
	};

	unpack_context->item.as.UInt = num;
	unpack_context->item.type = CPCP_ITEM_UINT;
	if (pbitlen)
		*pbitlen = bitlen;
}

static void unpack_int(cpcp_unpack_context *unpack_context, int64_t *pval) {
	int64_t snum = 0;
	int bitlen;
	unpack_uint(unpack_context, &bitlen);
	if (unpack_context->err_no == CPCP_RC_OK) {
		const uint64_t sign_bit_mask = (uint64_t)1 << (bitlen - 1);
		uint64_t num = unpack_context->item.as.UInt;
		snum = (int64_t)num;

		// Note: masked value assignment to bool variable would be undefined on
		// some platforms.

		if (num & sign_bit_mask) {
			snum &= ~(int64_t)sign_bit_mask;
			snum = -snum;
		}
	}
	if (pval)
		*pval = snum;
}

void unpack_string(cpcp_unpack_context *unpack_context) {
	const char *p;
	cpcp_string *it = &unpack_context->item.as.String;

	bool is_cstr = it->string_size < 0;
	if (is_cstr) {
		for (it->chunk_size = 0; it->chunk_size < it->chunk_buff_len;) {
			UNPACK_TAKE_BYTE(p);
			if (*p == '\\') {
				UNPACK_TAKE_BYTE(p);
				if (!p)
					return;
				switch (*p) {
					case '\\':
						(it->chunk_start)[it->chunk_size++] = '\\';
						break;
					case '0':
						(it->chunk_start)[it->chunk_size++] = '\0';
						break;
					default:
						(it->chunk_start)[it->chunk_size++] = *p;
						break;
				}
				break;
			}
			if (*p == '\0') {
				// end of string
				it->last_chunk = 1;
				break;
			}

			(it->chunk_start)[it->chunk_size++] = *p;
		}
	} else {
		it->chunk_size = 0;
		while (it->size_to_load > 0 && it->chunk_size < it->chunk_buff_len) {
			UNPACK_TAKE_BYTE(p);
			(it->chunk_start)[it->chunk_size++] = *p;
			it->size_to_load--;
		}
		it->last_chunk = (it->size_to_load == 0);
	}
	it->chunk_cnt++;
	unpack_context->item.type = CPCP_ITEM_STRING;
}

void unpack_blob(cpcp_unpack_context *unpack_context) {
	unpack_string(unpack_context);
	unpack_context->item.type = CPCP_ITEM_BLOB;
}

void chainpack_unpack_next(cpcp_unpack_context *unpack_context) {
	if (unpack_context->err_no)
		return;

	if (unpack_context->item.type == CPCP_ITEM_STRING ||
		unpack_context->item.type == CPCP_ITEM_BLOB) {
		cpcp_string *str_it = &unpack_context->item.as.String;
		if (!str_it->last_chunk) {
			if (unpack_context->item.type == CPCP_ITEM_STRING)
				unpack_string(unpack_context);
			else
				unpack_blob(unpack_context);
			return;
		}
	}

	const char *p;
	UNPACK_TAKE_BYTE(p);

	uint8_t packing_schema = (uint8_t)(*p);

	cpcp_container_state *top_cont_state =
		cpcp_unpack_context_top_container_state(unpack_context);
	if (top_cont_state && packing_schema != CP_TERM) {
		top_cont_state->item_count++;
	}

	unpack_context->item.type = CPCP_ITEM_INVALID;
	if (packing_schema < 128) {
		if (packing_schema & 64) {
			// tiny Int
			unpack_context->item.type = CPCP_ITEM_INT;
			unpack_context->item.as.Int = packing_schema & 63;
		} else {
			// tiny UInt
			unpack_context->item.type = CPCP_ITEM_UINT;
			unpack_context->item.as.UInt = packing_schema & 63;
		}
	} else {
		switch (packing_schema) {
			case CP_Null: {
				unpack_context->item.type = CPCP_ITEM_NULL;
				break;
			}
			case CP_TRUE: {
				unpack_context->item.type = CPCP_ITEM_BOOLEAN;
				unpack_context->item.as.Bool = 1;
				break;
			}
			case CP_FALSE: {
				unpack_context->item.type = CPCP_ITEM_BOOLEAN;
				unpack_context->item.as.Bool = 0;
				break;
			}
			case CP_Int: {
				int64_t n;
				unpack_int(unpack_context, &n);
				unpack_context->item.type = CPCP_ITEM_INT;
				unpack_context->item.as.Int = n;
				break;
			}
			case CP_UInt: {
				unpack_uint(unpack_context, NULL);
				break;
			}
			case CP_Double: {
				unpack_context->item.type = CPCP_ITEM_DOUBLE;
				uint8_t *bytes = (uint8_t *)&(unpack_context->item.as.Double);
				int len = sizeof(double);

				int n = 1;
				int i;
				if (*(char *)&n == 1) {
					// little endian if true
					for (i = 0; i < len; i++) {
						UNPACK_TAKE_BYTE(p);
						bytes[i] = (uint8_t)(*p);
					}
				} else {
					for (i = len - 1; i >= 0; i--) {
						UNPACK_TAKE_BYTE(p);
						bytes[i] = (uint8_t)(*p);
					}
				}
				break;
			}
			case CP_Decimal: {
				int64_t mant;
				unpack_int(unpack_context, &mant);
				int64_t exp;
				unpack_int(unpack_context, &exp);
				unpack_context->item.type = CPCP_ITEM_DECIMAL;
				unpack_context->item.as.Decimal.mantisa = mant;
				unpack_context->item.as.Decimal.exponent = (int)exp;
				break;
			}
			case CP_DateTime: {
				int64_t d;
				unpack_int(unpack_context, &d);
				int8_t offset = 0;
				bool has_tz_offset = d & 1;
				bool has_not_msec = d & 2;
				d >>= 2;
				if (has_tz_offset) {
					offset = d & 0x7F;
					offset = (int8_t)(offset << 1);
					offset >>= 1; // sign extension
					d >>= 7;
				}
				if (has_not_msec)
					d *= 1000;
				d += SHV_EPOCH_MSEC;

				unpack_context->item.type = CPCP_ITEM_DATE_TIME;
				cpcp_date_time *it = &unpack_context->item.as.DateTime;
				it->msecs_since_epoch = d;
				it->minutes_from_utc = offset * (int)15;
				break;
			}
			case CP_MetaMap: {
				unpack_context->item.type = CPCP_ITEM_META;
				cpcp_unpack_context_push_container_state(
					unpack_context, unpack_context->item.type);
				break;
			}
			case CP_Map: {
				unpack_context->item.type = CPCP_ITEM_MAP;
				cpcp_unpack_context_push_container_state(
					unpack_context, unpack_context->item.type);
				break;
			}
			case CP_IMap: {
				unpack_context->item.type = CPCP_ITEM_IMAP;
				cpcp_unpack_context_push_container_state(
					unpack_context, unpack_context->item.type);
				break;
			}
			case CP_List: {
				unpack_context->item.type = CPCP_ITEM_LIST;
				cpcp_unpack_context_push_container_state(
					unpack_context, unpack_context->item.type);
				break;
			}
			case CP_TERM: {
				unpack_context->item.type = CPCP_ITEM_CONTAINER_END;
				cpcp_unpack_context_pop_container_state(unpack_context);
				break;
			}
			case CP_Blob: {
				cpcp_string *it = &unpack_context->item.as.String;
				cpcp_string_init(it, unpack_context);
				unpack_uint(unpack_context, NULL);
				if (unpack_context->err_no == CPCP_RC_OK) {
					it->string_size = (long)(unpack_context->item.as.UInt);
					it->size_to_load = it->string_size;
					unpack_blob(unpack_context);
				}
				break;
			}
			case CP_String: {
				cpcp_string *it = &unpack_context->item.as.String;
				cpcp_string_init(it, unpack_context);
				unpack_uint(unpack_context, NULL);
				if (unpack_context->err_no == CPCP_RC_OK) {
					it->string_size = (long)(unpack_context->item.as.UInt);
					it->size_to_load = it->string_size;
					unpack_string(unpack_context);
				}
				break;
			}
			case CP_CString: {
				cpcp_string *it = &unpack_context->item.as.String;
				cpcp_string_init(it, unpack_context);
				it->string_size = -1;
				it->size_to_load = it->string_size;
				unpack_string(unpack_context);
				break;
			}
			default:
				UNPACK_ERROR(CPCP_RC_MALFORMED_INPUT, "Invalid type info.");
		}
	}
}

uint64_t chainpack_unpack_uint_data(cpcp_unpack_context *unpack_context, bool *ok) {
	int err_code;
	uint64_t n = chainpack_unpack_uint_data2(unpack_context, &err_code);
	if (ok)
		*ok = (err_code == CPCP_RC_OK);
	return n;
}

uint64_t chainpack_unpack_uint_data2(
	cpcp_unpack_context *unpack_context, int *err_code) {
	unpack_uint(unpack_context, NULL);
	if (err_code)
		*err_code = unpack_context->err_no;
	return unpack_context->item.as.UInt;
}
