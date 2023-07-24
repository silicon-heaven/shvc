#ifndef SHV_CPCP_UNPACK_H
#define SHV_CPCP_UNPACK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


struct cpcp_unpack_context;

typedef enum {
	CPCP_ITEM_INVALID = 0,
	CPCP_ITEM_NULL,
	CPCP_ITEM_BOOLEAN,
	CPCP_ITEM_INT,
	CPCP_ITEM_UINT,
	CPCP_ITEM_DOUBLE,
	CPCP_ITEM_DECIMAL,
	CPCP_ITEM_BLOB,
	CPCP_ITEM_STRING,
	CPCP_ITEM_DATE_TIME,

	CPCP_ITEM_LIST,
	CPCP_ITEM_MAP,
	CPCP_ITEM_IMAP,
	CPCP_ITEM_META,
	CPCP_ITEM_CONTAINER_END,
} cpcp_item_types;

const char *cpcp_item_type_to_string(cpcp_item_types t);

#ifndef CPCP_MAX_STRING_KEY_LEN
#define CPCP_STRING_CHUNK_BUFF_LEN 256
#endif

typedef struct {
	long string_size;
	long size_to_load;
	char *chunk_start;
	size_t chunk_size;
	size_t chunk_buff_len;
	unsigned chunk_cnt;
	uint8_t last_chunk:1, blob_hex:1;
} cpcp_string;

void cpcp_string_init(
	cpcp_string *str_it, struct cpcp_unpack_context *unpack_context);

typedef struct {
	int64_t msecs_since_epoch;
	int minutes_from_utc;
} cpcp_date_time;

typedef struct {
	int64_t mantisa;
	int exponent;
} cpcp_decimal;

double cpcp_exponentional_to_double(
	const int64_t mantisa, const int exponent, const int base);
double cpcp_decimal_to_double(const int64_t mantisa, const int exponent);
size_t cpcp_decimal_to_string(
	char *buff, size_t buff_len, int64_t mantisa, int exponent);

typedef struct {
	union {
		cpcp_string String;
		cpcp_date_time DateTime;
		cpcp_decimal Decimal;
		uint64_t UInt;
		int64_t Int;
		double Double;
		bool Bool;
	} as;
	cpcp_item_types type;
} cpcp_item;

typedef struct {
	cpcp_item_types container_type;
	size_t item_count;
	cpcp_item_types current_item_type;
} cpcp_container_state;

void cpcp_container_state_init(
	cpcp_container_state *self, cpcp_item_types cont_type);

struct cpcp_container_stack;

typedef int (*cpcp_container_stack_overflow_handler)(struct cpcp_container_stack *);

typedef struct cpcp_container_stack {
	cpcp_container_state *container_states;
	size_t length;
	size_t capacity;
	cpcp_container_stack_overflow_handler overflow_handler;
} cpcp_container_stack;

void cpcp_container_stack_init(cpcp_container_stack *self,
	cpcp_container_state *states, size_t capacity,
	cpcp_container_stack_overflow_handler hnd);

typedef size_t (*cpcp_unpack_underflow_handler)(struct cpcp_unpack_context *);

typedef struct cpcp_unpack_context {
	cpcp_item item;
	const char *start;
	const char *current;
	const char *end;	/* logical end of buffer */
	int err_no;			/* handlers can save error here */
	int parser_line_no; /* helps to find parser error line */
	const char *err_msg;
	cpcp_container_stack *container_stack;
	cpcp_unpack_underflow_handler handle_unpack_underflow;
	char default_string_chunk_buff[CPCP_STRING_CHUNK_BUFF_LEN];
	char *string_chunk_buff;
	size_t string_chunk_buff_len;
} cpcp_unpack_context;

void cpcp_unpack_context_init(cpcp_unpack_context *self, const void *data,
	size_t length, cpcp_unpack_underflow_handler huu, cpcp_container_stack *stack);

cpcp_container_state *cpcp_unpack_context_push_container_state(
	cpcp_unpack_context *self, cpcp_item_types container_type);
cpcp_container_state *cpcp_unpack_context_top_container_state(
	cpcp_unpack_context *self);
cpcp_container_state *cpcp_unpack_context_parent_container_state(
	cpcp_unpack_context *self);
cpcp_container_state *cpcp_unpack_context_closed_container_state(
	cpcp_unpack_context *self);
void cpcp_unpack_context_pop_container_state(cpcp_unpack_context *self);

const char *cpcp_unpack_take_byte(cpcp_unpack_context *unpack_context);
const char *cpcp_unpack_peek_byte(cpcp_unpack_context *unpack_context);
#define UNPACK_ERROR(error_code, error_msg) \
	{ \
		unpack_context->item.type = CPCP_ITEM_INVALID; \
		unpack_context->err_no = error_code; \
		unpack_context->err_msg = error_msg; \
		return; \
	}

#define UNPACK_TAKE_BYTE(p) \
	{ \
		p = cpcp_unpack_take_byte(unpack_context); \
		if (!p) \
			return; \
	}

#define UNPACK_PEEK_BYTE(p) \
	{ \
		p = cpcp_unpack_peek_byte(unpack_context); \
		if (!p) \
			return; \
	}


#endif
