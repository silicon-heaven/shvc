#include <shv/chainpack.h>
#include <shv/cpon.h>

#include <assert.h>

void cpcp_convert(cpcp_unpack_context *in_ctx, enum cpcp_format in_format,
	cpcp_pack_context *out_ctx, enum cpcp_format out_format) {
	if (!in_ctx->container_stack) {
		// cpcp_convert() cannot worj without input context container state set
		in_ctx->err_no = CPCP_RC_LOGICAL_ERROR;
		return;
	}
	bool o_cpon_input = (in_format == CPCP_Cpon);
	bool o_chainpack_output = (out_format == CPCP_ChainPack);
	bool meta_just_closed = false;
	do {
		if (o_cpon_input)
			cpon_unpack_next(in_ctx);
		else
			chainpack_unpack_next(in_ctx);
		if (in_ctx->err_no != CPCP_RC_OK)
			break;

		cpcp_container_state *parent_state =
			cpcp_unpack_context_parent_container_state(in_ctx);
		if (o_chainpack_output) {
		} else {
			if (parent_state != NULL) {
				bool is_string_concat = 0;
				if (in_ctx->item.type == CPCP_ITEM_STRING) {
					cpcp_string *it = &(in_ctx->item.as.String);
					if (it->chunk_cnt > 1) {
						// multichunk string
						// this can happen, when parsed string is greater than
						// unpack_context buffer or escape sequence is
						// encountered concatenate it with previous chunk
						is_string_concat = 1;
					}
				}
				if (!is_string_concat &&
					in_ctx->item.type != CPCP_ITEM_CONTAINER_END) {
					switch (parent_state->container_type) {
						case CPCP_ITEM_LIST:
							if (!meta_just_closed)
								cpon_pack_field_delim(out_ctx,
									parent_state->item_count == 1, false);
							break;
						case CPCP_ITEM_MAP:
						case CPCP_ITEM_IMAP:
						case CPCP_ITEM_META: {
							bool is_key = (parent_state->item_count % 2);
							if (is_key) {
								if (!meta_just_closed)
									cpon_pack_field_delim(out_ctx,
										parent_state->item_count == 1, false);
							} else {
								// delimite value
								if (!meta_just_closed)
									cpon_pack_key_val_delim(out_ctx);
							}
							break;
						}
						default:
							break;
					}
				}
			}
		}
		meta_just_closed = false;
		switch (in_ctx->item.type) {
			case CPCP_ITEM_INVALID: {
				// end of input
				break;
			}
			case CPCP_ITEM_NULL: {
				if (o_chainpack_output)
					chainpack_pack_null(out_ctx);
				else
					cpon_pack_null(out_ctx);
				break;
			}
			case CPCP_ITEM_LIST: {
				if (o_chainpack_output)
					chainpack_pack_list_begin(out_ctx);
				else
					cpon_pack_list_begin(out_ctx);
				break;
			}
			case CPCP_ITEM_MAP: {
				if (o_chainpack_output)
					chainpack_pack_map_begin(out_ctx);
				else
					cpon_pack_map_begin(out_ctx);
				break;
			}
			case CPCP_ITEM_IMAP: {
				if (o_chainpack_output)
					chainpack_pack_imap_begin(out_ctx);
				else
					cpon_pack_imap_begin(out_ctx);
				break;
			}
			case CPCP_ITEM_META: {
				if (o_chainpack_output)
					chainpack_pack_meta_begin(out_ctx);
				else
					cpon_pack_meta_begin(out_ctx);
				break;
			}
			case CPCP_ITEM_CONTAINER_END: {
				cpcp_container_state *st =
					cpcp_unpack_context_closed_container_state(in_ctx);
				if (!st) {
					in_ctx->err_no = CPCP_RC_CONTAINER_STACK_UNDERFLOW;
					return;
				}
				meta_just_closed = (st->container_type == CPCP_ITEM_META);

				if (o_chainpack_output) {
					chainpack_pack_container_end(out_ctx);
				} else {
					switch (st->container_type) {
						case CPCP_ITEM_LIST:
							cpon_pack_list_end(out_ctx, false);
							break;
						case CPCP_ITEM_MAP:
							cpon_pack_map_end(out_ctx, false);
							break;
						case CPCP_ITEM_IMAP:
							cpon_pack_imap_end(out_ctx, false);
							break;
						case CPCP_ITEM_META:
							cpon_pack_meta_end(out_ctx, false);
							break;
						default:
							// cannot finish Cpon container without container
							// type info
							in_ctx->err_no = CPCP_RC_LOGICAL_ERROR;
							return;
					}
				}
				break;
			}
			case CPCP_ITEM_BLOB: {
				cpcp_string *it = &in_ctx->item.as.String;
				if (o_chainpack_output) {
					if (it->chunk_cnt == 1 && it->last_chunk) {
						// one chunk string with known length is always packed
						// as RAW
						chainpack_pack_blob(out_ctx, (uint8_t *)it->chunk_start,
							it->chunk_size);
					} else if (it->string_size >= 0) {
						if (it->chunk_cnt == 1)
							chainpack_pack_blob_start(out_ctx,
								(size_t)(it->string_size),
								(uint8_t *)it->chunk_start,
								(size_t)(it->string_size));
						else
							chainpack_pack_blob_cont(out_ctx,
								(uint8_t *)it->chunk_start, it->chunk_size);
					} else {
						// cstring
						// not supported, there is nothing like CBlob
					}
				} else {
					// Cpon
					if (it->chunk_cnt == 1)
						cpon_pack_blob_start(out_ctx,
							(uint8_t *)it->chunk_start, it->chunk_size);
					else
						cpon_pack_blob_cont(out_ctx, (uint8_t *)it->chunk_start,
							(unsigned)(it->chunk_size));
					if (it->last_chunk)
						cpon_pack_blob_finish(out_ctx);
				}
				break;
			}
			case CPCP_ITEM_STRING: {
				cpcp_string *it = &in_ctx->item.as.String;
				if (o_chainpack_output) {
					if (it->chunk_cnt == 1 && it->last_chunk) {
						// one chunk string with known length is always packed
						// as RAW
						chainpack_pack_string(
							out_ctx, it->chunk_start, it->chunk_size);
					} else if (it->string_size >= 0) {
						if (it->chunk_cnt == 1)
							chainpack_pack_string_start(out_ctx,
								(size_t)(it->string_size), it->chunk_start,
								(size_t)(it->string_size));
						else
							chainpack_pack_string_cont(
								out_ctx, it->chunk_start, it->chunk_size);
					} else {
						// cstring
						if (it->chunk_cnt == 1)
							chainpack_pack_cstring_start(
								out_ctx, it->chunk_start, it->chunk_size);
						else
							chainpack_pack_cstring_cont(
								out_ctx, it->chunk_start, it->chunk_size);
						if (it->last_chunk)
							chainpack_pack_cstring_finish(out_ctx);
					}
				} else {
					// Cpon
					if (it->chunk_cnt == 1)
						cpon_pack_string_start(
							out_ctx, it->chunk_start, it->chunk_size);
					else
						cpon_pack_string_cont(out_ctx, it->chunk_start,
							(unsigned)(it->chunk_size));
					if (it->last_chunk)
						cpon_pack_string_finish(out_ctx);
				}
				break;
			}
			case CPCP_ITEM_BOOLEAN: {
				if (o_chainpack_output)
					chainpack_pack_boolean(out_ctx, in_ctx->item.as.Bool);
				else
					cpon_pack_boolean(out_ctx, in_ctx->item.as.Bool);
				break;
			}
			case CPCP_ITEM_INT: {
				if (o_chainpack_output)
					chainpack_pack_int(out_ctx, in_ctx->item.as.Int);
				else
					cpon_pack_int(out_ctx, in_ctx->item.as.Int);
				break;
			}
			case CPCP_ITEM_UINT: {
				if (o_chainpack_output)
					chainpack_pack_uint(out_ctx, in_ctx->item.as.UInt);
				else
					cpon_pack_uint(out_ctx, in_ctx->item.as.UInt);
				break;
			}
			case CPCP_ITEM_DECIMAL: {
				if (o_chainpack_output)
					chainpack_pack_decimal(out_ctx, &in_ctx->item.as.Decimal);
				else
					cpon_pack_decimal(out_ctx, &in_ctx->item.as.Decimal);
				break;
			}
			case CPCP_ITEM_DOUBLE: {
				if (o_chainpack_output)
					chainpack_pack_double(out_ctx, in_ctx->item.as.Double);
				else
					cpon_pack_double(out_ctx, in_ctx->item.as.Double);
				break;
			}
			case CPCP_ITEM_DATE_TIME: {
				cpcp_date_time *it = &in_ctx->item.as.DateTime;
				if (o_chainpack_output)
					chainpack_pack_date_time(out_ctx, it);
				else
					cpon_pack_date_time(out_ctx, it);
				break;
			}
		}
		{
			cpcp_container_state *top_state =
				cpcp_unpack_context_top_container_state(in_ctx);
			// take just one object from stream
			if (!top_state) {
				if (((in_ctx->item.type == CPCP_ITEM_STRING ||
						 in_ctx->item.type == CPCP_ITEM_BLOB) &&
						!in_ctx->item.as.String.last_chunk) ||
					meta_just_closed) {
					// do not stop parsing in the middle of the string
					// or after meta
				} else {
					break;
				}
			}
		}
	} while (in_ctx->err_no == CPCP_RC_OK && out_ctx->err_no == CPCP_RC_OK);

	if (out_ctx->handle_pack_overflow)
		out_ctx->handle_pack_overflow(out_ctx, 0);
}
