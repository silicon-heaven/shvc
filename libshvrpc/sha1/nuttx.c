#include "sha1.h"
#include <stdlib.h>
#include <assert.h>
#include <crypto/sha1.h>

struct sha1ctx {
	SHA1_CTX ctx;
};

sha1ctx_t sha1_new() {
	struct sha1ctx *ctx = malloc(sizeof *ctx);
	sha1init(&ctx->ctx);
	return ctx;
}

void sha1_destroy(sha1ctx_t ctx) {
	free(ctx);
}

bool sha1_update(sha1ctx_t ctx, const uint8_t *buf, size_t siz) {
	sha1update(&ctx->ctx, (const void *)buf, siz);
	return true;
}

bool sha1_hex_digest(sha1ctx_t ctx, char digest[SHA1_HEX_SIZ]) {
	unsigned char buf[SHA1_DIGEST_LENGTH];
	sha1final(buf, &ctx->ctx);
	static const char *const hex = "0123456789abcdef";
	for (unsigned i = 0; i < SHA1_DIGEST_LENGTH; i++) {
		*digest++ = hex[(buf[i] >> 4) & 0xf];
		*digest++ = hex[buf[i] & 0xf];
	}
	return true;
}
