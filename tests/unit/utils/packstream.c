#include "packstream.h"
#include "shv/cp_pack.h"
#include <check.h>

char *packbuf;
size_t packbufsiz;
FILE *packstream;


void setup_packstream(void) {
	packbufsiz = 0;
	packstream = open_memstream(&packbuf, &packbufsiz);
	ck_assert_ptr_nonnull(packstream);
}

void teardown_packstream(void) {
	fclose(packstream);
	free(packbuf);
}


static struct cp_pack_chainpack pack_chainpack;
static struct cp_pack_cpon pack_cpon;
cp_pack_t packstream_pack;

void setup_packstream_pack_chainpack(void) {
	setup_packstream();
	packstream_pack = cp_pack_chainpack_init(&pack_chainpack, packstream);
}

void setup_packstream_pack_cpon(void) {
	setup_packstream();
	packstream_pack = cp_pack_cpon_init(&pack_cpon, packstream, "");
}

void teardown_packstream_pack(void) {
	if (packstream_pack == &pack_cpon.func)
		free(pack_cpon.state.ctx);
	teardown_packstream();
}
