#include <stdbool.h>
#include <shv/crc32.h>

#define SUITE "crc32"
#include <check_suite.h>

static crc32_t refcupdate(crc32_t crc, uint8_t *data, size_t len) {
	while (len--) {
		crc = crc ^ *data;
		for (int j = 7; j >= 0; j--) {
			uint32_t mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
		data++;
	}
	return crc;
}



TEST_CASE(all) {}

TEST(all, single_byte) {
	for (unsigned v = 0; v <= UINT8_MAX; v++) {
		uint32_t val =
			crc32_finalize(crc32_update(crc32_init(), (uint8_t *)&v, 1));
		crc32_t rval = crc32_finalize(refcupdate(crc32_init(), (uint8_t *)&v, 1));
		ck_assert_int_eq(val, rval);
	}
}
