#ifndef SHV_CPCP_PACK_H
#define SHV_CPCP_PACK_H

#include <stddef.h>
#include <stdint.h>


typedef struct {
	const char *indent;
	uint8_t json_output:1;
} cpcp_cpon_pack_options;

struct cpcp_pack_context;

typedef void (*cpcp_pack_overflow_handler)(
	struct cpcp_pack_context *, size_t size_hint);

typedef struct cpcp_pack_context {
	char *start;
	char *current;
	char *end;
	int nest_count;
	int err_no; /* handlers can save error here */
	cpcp_pack_overflow_handler handle_pack_overflow;
	cpcp_cpon_pack_options cpon_options;
	size_t bytes_written;
} cpcp_pack_context;

void cpcp_pack_context_init(cpcp_pack_context *pack_context, void *data,
	size_t length, cpcp_pack_overflow_handler poh);
void cpcp_pack_context_dry_run_init(cpcp_pack_context *pack_context);

size_t cpcp_pack_copy_byte(cpcp_pack_context *pack_context, uint8_t b);
size_t cpcp_pack_copy_bytes(
	cpcp_pack_context *pack_context, const void *str, size_t len);


#endif
