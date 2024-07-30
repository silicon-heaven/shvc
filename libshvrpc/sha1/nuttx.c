#include <stdlib.h>
#include <assert.h>
#include <crypto/sha1.h>
#include <shv/sha1.h>

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

bool sha1_digest(sha1ctx_t ctx, uint8_t digest[SHA1_SIZ]) {
	sha1final(digest, &ctx->ctx);
	return true;
}
