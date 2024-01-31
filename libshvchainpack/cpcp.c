#include <shv/cpcp.h>
#include <string.h>


const char *cpcp_error_string(enum cpcp_error errno) {
	switch (errno) {
		case CPCP_RC_OK:
			return "";
		case CPCP_RC_MALLOC_ERROR:
			return "MALLOC_ERROR";
		case CPCP_RC_BUFFER_OVERFLOW:
			return "BUFFER_OVERFLOW";
		case CPCP_RC_BUFFER_UNDERFLOW:
			return "BUFFER_UNDERFLOW";
		case CPCP_RC_MALFORMED_INPUT:
			return "MALFORMED_INPUT";
		case CPCP_RC_LOGICAL_ERROR:
			return "LOGICAL_ERROR";
		case CPCP_RC_CONTAINER_STACK_OVERFLOW:
			return "CONTAINER_STACK_OVERFLOW";
		case CPCP_RC_CONTAINER_STACK_UNDERFLOW:
			return "CONTAINER_STACK_UNDERFLOW";
		default:
			return "UNKNOWN";
	}
}

//=========================== PACK ============================
void cpcp_pack_context_init(cpcp_pack_context *pack_context, void *data,
	size_t length, cpcp_pack_overflow_handler poh) {
	pack_context->start = pack_context->current = (char *)data;
	pack_context->end = pack_context->start;
	if (length) {
		pack_context->end += length;
	}
	pack_context->err_no = 0;
	pack_context->handle_pack_overflow = poh;
	pack_context->err_no = CPCP_RC_OK;
	pack_context->cpon_options.indent = NULL;
	pack_context->cpon_options.json_output = 0;
	pack_context->nest_count = 0;
	pack_context->bytes_written = 0;
}

void cpcp_pack_context_dry_run_init(cpcp_pack_context *pack_context) {
	cpcp_pack_context_init(pack_context, NULL, 0, NULL);
}

static bool is_dry_run(cpcp_pack_context *pack_context) {
	return pack_context->handle_pack_overflow == NULL &&
		pack_context->start == NULL && pack_context->end == NULL &&
		pack_context->current == NULL;
}

// try to make size_hint bytes space in pack_context
// returns number of bytes available in pack_context buffer, can be < size_hint,
// but always > 0 returns 0 if fails
static size_t cpcp_pack_make_space(
	cpcp_pack_context *pack_context, size_t size_hint) {
	if (pack_context->err_no != CPCP_RC_OK)
		return 0;
	size_t free_space = (size_t)(pack_context->end - pack_context->current);
	if (free_space < size_hint) {
		if (!pack_context->handle_pack_overflow) {
			pack_context->err_no = CPCP_RC_BUFFER_OVERFLOW;
			return 0;
		}
		pack_context->handle_pack_overflow(pack_context, size_hint);
		free_space = (size_t)(pack_context->end - pack_context->current);
		if (free_space < 1) {
			pack_context->err_no = CPCP_RC_BUFFER_OVERFLOW;
			return 0;
		}
	}
	return free_space;
}

// alocate more bytes, move current after allocated bytec
static char *cpcp_pack_reserve_space(cpcp_pack_context *pack_context, size_t more) {
	if (pack_context->err_no != CPCP_RC_OK)
		return NULL;
	size_t free_space = cpcp_pack_make_space(pack_context, more);
	if (free_space < more) {
		pack_context->err_no = CPCP_RC_BUFFER_OVERFLOW;
		return NULL;
	}
	char *p = pack_context->current;
	pack_context->current = p + more;
	return p;
}

size_t cpcp_pack_copy_byte(cpcp_pack_context *pack_context, uint8_t b) {
	pack_context->bytes_written += 1;
	if (is_dry_run(pack_context))
		return 1;

	char *p = cpcp_pack_reserve_space(pack_context, 1);
	if (!p)
		return 0;
	*p = (char)b;
	return 1;
}

size_t cpcp_pack_copy_bytes(
	cpcp_pack_context *pack_context, const void *str, size_t len) {
	pack_context->bytes_written += len;
	if (is_dry_run(pack_context))
		return len;

	size_t copied = 0;
	while (pack_context->err_no == CPCP_RC_OK && copied < len) {
		size_t buff_size = cpcp_pack_make_space(pack_context, len);
		if (buff_size == 0) {
			pack_context->err_no = CPCP_RC_BUFFER_OVERFLOW;
			return 0;
		}
		size_t rest = len - copied;
		if (rest > buff_size)
			rest = buff_size;
		memcpy(pack_context->current, ((const char *)str) + copied, rest);
		copied += rest;
		pack_context->current += rest;
	}
	return copied;
}

//================================ UNPACK ================================
const char *cpcp_item_type_to_string(cpcp_item_types t) {
	switch (t) {
		case CPCP_ITEM_INVALID:
			break;
		case CPCP_ITEM_NULL:
			return "NULL";
		case CPCP_ITEM_BOOLEAN:
			return "BOOLEAN";
		case CPCP_ITEM_INT:
			return "INT";
		case CPCP_ITEM_UINT:
			return "UINT";
		case CPCP_ITEM_DOUBLE:
			return "DOUBLE";
		case CPCP_ITEM_DECIMAL:
			return "DECIMAL";
		case CPCP_ITEM_BLOB:
			return "BLOB";
		case CPCP_ITEM_STRING:
			return "STRING";
		case CPCP_ITEM_DATE_TIME:
			return "DATE_TIME";
		case CPCP_ITEM_LIST:
			return "LIST";
		case CPCP_ITEM_MAP:
			return "MAP";
		case CPCP_ITEM_IMAP:
			return "IMAP";
		case CPCP_ITEM_META:
			return "META";
		case CPCP_ITEM_CONTAINER_END:
			return "CONTAINER_END";
	}
	return "INVALID";
}

void cpcp_container_state_init(
	cpcp_container_state *self, cpcp_item_types cont_type) {
	self->container_type = cont_type;
	self->current_item_type = CPCP_ITEM_INVALID;
	self->item_count = 0;
}

void cpcp_container_stack_init(cpcp_container_stack *self,
	cpcp_container_state *states, size_t capacity,
	cpcp_container_stack_overflow_handler hnd) {
	self->container_states = states;
	self->capacity = capacity;
	self->length = 0;
	self->overflow_handler = hnd;
}

void cpcp_unpack_context_init(cpcp_unpack_context *self, const void *data,
	size_t length, cpcp_unpack_underflow_handler huu, cpcp_container_stack *stack) {
	self->item.type = CPCP_ITEM_INVALID;
	self->start = self->current = (const char *)data;
	self->end = self->start + length;
	self->err_no = CPCP_RC_OK;
	self->parser_line_no = 1;
	self->err_msg = "";
	self->handle_unpack_underflow = huu;
	self->container_stack = stack;
	self->string_chunk_buff = self->default_string_chunk_buff;
	self->string_chunk_buff_len = sizeof(self->default_string_chunk_buff);
}

cpcp_container_state *cpcp_unpack_context_push_container_state(
	cpcp_unpack_context *self, cpcp_item_types container_type) {
	if (!self->container_stack) {
		// C++ implementation does not require container states stack
		return NULL;
	}
	if (self->container_stack->length == self->container_stack->capacity) {
		if (!self->container_stack->overflow_handler) {
			self->err_no = CPCP_RC_CONTAINER_STACK_OVERFLOW;
			return NULL;
		}
		int rc = self->container_stack->overflow_handler(self->container_stack);
		if (rc < 0) {
			self->err_no = CPCP_RC_CONTAINER_STACK_OVERFLOW;
			return NULL;
		}
	}
	if (self->container_stack->length < self->container_stack->capacity - 1) {
		cpcp_container_state *state = self->container_stack->container_states +
			self->container_stack->length;
		cpcp_container_state_init(state, container_type);
		self->container_stack->length++;
		return state;
	}
	self->err_no = CPCP_RC_CONTAINER_STACK_OVERFLOW;
	return NULL;
}

cpcp_container_state *cpcp_unpack_context_top_container_state(
	cpcp_unpack_context *self) {
	if (self->container_stack && self->container_stack->length > 0) {
		return self->container_stack->container_states +
			self->container_stack->length - 1;
	}
	return NULL;
}

cpcp_container_state *cpcp_unpack_context_parent_container_state(
	cpcp_unpack_context *self) {
	if (self->container_stack && self->container_stack->length > 0) {
		cpcp_container_state *top_st = self->container_stack->container_states +
			self->container_stack->length - 1;
		if (top_st && top_st->item_count == 0) {
			if (self->container_stack->length > 1)
				return self->container_stack->container_states +
					self->container_stack->length - 2;

			return NULL;
		}
		return top_st;
	}
	return NULL;
}

cpcp_container_state *cpcp_unpack_context_closed_container_state(
	cpcp_unpack_context *self) {
	if (self->container_stack && self->item.type == CPCP_ITEM_CONTAINER_END) {
		cpcp_container_state *st = self->container_stack->container_states +
			self->container_stack->length;
		return st;
	}
	return NULL;
}

void cpcp_unpack_context_pop_container_state(cpcp_unpack_context *self) {
	if (self->container_stack && self->container_stack->length > 0) {
		self->container_stack->length--;
	}
}

void cpcp_string_init(cpcp_string *self, cpcp_unpack_context *unpack_context) {
	self->chunk_size = 0;
	self->chunk_cnt = 0;
	self->last_chunk = 0;
	self->string_size = -1;
	self->size_to_load = -1;
	self->chunk_start = unpack_context->string_chunk_buff;
	self->chunk_buff_len = unpack_context->string_chunk_buff_len;
	self->blob_hex = 0;
}

const char *cpcp_unpack_take_byte(cpcp_unpack_context *unpack_context) {
	const char *p = cpcp_unpack_peek_byte(unpack_context);
	if (p)
		unpack_context->current++;
	return p;
}

const char *cpcp_unpack_peek_byte(cpcp_unpack_context *unpack_context) {
	static const size_t more = 1;
	if (unpack_context->current >= unpack_context->end) {
		if (!unpack_context->handle_unpack_underflow) {
			unpack_context->err_no = CPCP_RC_BUFFER_UNDERFLOW;
			return NULL;
		}
		size_t sz = unpack_context->handle_unpack_underflow(unpack_context);
		if (sz < more) {
			unpack_context->err_no = CPCP_RC_BUFFER_UNDERFLOW;
			return NULL;
		}
		unpack_context->current = unpack_context->start;
	}
	const char *p = unpack_context->current;
	return p;
}

double cpcp_exponentional_to_double(
	int64_t const mantisa, const int exponent, const int base) {
	double d = (double)mantisa;
	int i;
	for (i = 0; i < exponent; ++i)
		d *= base;
	for (i = exponent; i < 0; ++i)
		d /= base;
	return d;
}

double cpcp_decimal_to_double(const int64_t mantisa, const int exponent) {
	return cpcp_exponentional_to_double(mantisa, exponent, 10);
}

static size_t int_to_str(char *buff, size_t buff_len, int64_t val) {
	size_t n = 0;
	bool neg = false;
	char *str = buff;
	size_t i;
	if (val < 0) {
		neg = true;
		val = -val;
		str = buff + 1;
		buff_len--;
	}
	if (val == 0) {
		if (n == buff_len)
			return 0;
		str[n++] = '0';
	} else
		while (val != 0) {
			int d = (int)(val % 10);
			val /= 10;
			if (n == buff_len)
				return 0;
			str[n++] = '0' + (char)d;
		}
	for (i = 0; i < n / 2; ++i) {
		char c = str[i];
		str[i] = str[n - i - 1];
		str[n - i - 1] = c;
	}
	if (neg) {
		buff[0] = '-';
		n++;
	}
	return n;
}

size_t cpcp_decimal_to_string(
	char *buff, size_t buff_len, int64_t mantisa, int exponent) {
	bool neg = false;
	if (mantisa < 0) {
		mantisa = -mantisa;
		neg = true;
	}

	// at least 21 characters for 64-bit types.
	char *str = buff;
	if (neg) {
		str++;
		buff_len--;
	}
	size_t mantisa_str_len = int_to_str(str, buff_len, mantisa);
	if (mantisa_str_len == 0) {
		return mantisa_str_len;
	}

	size_t dec_places = (exponent < 0) ? (size_t)(-exponent) : 0;
	if (dec_places > 0 && dec_places < mantisa_str_len) {
		size_t dot_ix = mantisa_str_len - dec_places;
		for (size_t i = dot_ix; i < mantisa_str_len; ++i)
			str[mantisa_str_len + dot_ix - i] =
				str[mantisa_str_len + dot_ix - i - 1];
		str[dot_ix] = '.';
		mantisa_str_len++;
	} else if (dec_places > 0 && dec_places <= 3) {
		size_t extra_0_cnt = dec_places - mantisa_str_len;
		for (size_t i = 0; i < mantisa_str_len; ++i)
			str[mantisa_str_len - i - 1 + extra_0_cnt + 2] =
				str[mantisa_str_len - i - 1];
		str[0] = '0';
		str[1] = '.';
		for (size_t i = 0; i < extra_0_cnt; ++i)
			str[2 + i] = '0';
		mantisa_str_len += extra_0_cnt + 2;
	} else if (exponent > 0 && mantisa_str_len + (unsigned)exponent <= 9) {
		for (size_t i = 0; i < (unsigned)exponent; ++i)
			str[mantisa_str_len++] = '0';
		str[mantisa_str_len++] = '.';
	} else if (exponent == 0) {
		str[mantisa_str_len++] = '.';
	} else {
		str[mantisa_str_len++] = 'e';
		size_t n2 = int_to_str(
			str + mantisa_str_len, buff_len - mantisa_str_len, exponent);
		if (n2 == 0) {
			return n2;
		}
		mantisa_str_len += n2;
	}
	if (neg) {
		buff[0] = '-';
		mantisa_str_len++;
	}
	return mantisa_str_len;
}
