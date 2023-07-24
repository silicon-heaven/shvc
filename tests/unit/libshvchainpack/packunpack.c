#include "packunpack.h"

struct cpcp_pack_context pack;
struct cpcp_unpack_context unpack;

uint8_t *stashbuf;
size_t stashsiz;

static void pack_overflow_handler(struct cpcp_pack_context *pack, size_t size_hint) {
	size_t off = pack->current - pack->start;
	stashsiz *= 2;
	uint8_t *buf = realloc(stashbuf, stashsiz);
	ck_assert_ptr_nonnull(buf);
	stashbuf = buf;
	pack->start = (char *)buf;
	pack->current = (char *)buf + off;
	pack->end = (char *)buf + stashsiz;
}

void setup_pack(void) {
	stashsiz = 2;
	stashbuf = malloc(stashsiz);
	cpcp_pack_context_init(&pack, stashbuf, stashsiz, pack_overflow_handler);
}

void teardown_pack(void) {
	free(stashbuf);
}
