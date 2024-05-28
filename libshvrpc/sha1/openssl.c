#include "sha1.h"
#include <openssl/evp.h>
#include <assert.h>

// TODO possibly remove one pointer dereference here by using EVP_MD_CTX
// directly.
struct sha1ctx {
	EVP_MD_CTX *ctx;
};

sha1ctx_t sha1_new() {
	struct sha1ctx *ctx = malloc(sizeof *ctx);
	ctx->ctx = EVP_MD_CTX_new();
	if (EVP_DigestInit(ctx->ctx, EVP_sha1()))
		return ctx;
	sha1_destroy(ctx);
	return NULL;
}

void sha1_destroy(sha1ctx_t ctx) {
	if (ctx)
		EVP_MD_CTX_free(ctx->ctx);
	free(ctx);
}

bool sha1_update(sha1ctx_t ctx, const uint8_t *buf, size_t siz) {
	return EVP_DigestUpdate(ctx->ctx, buf, siz);
}

bool sha1_hex_digest(sha1ctx_t ctx, char digest[SHA1_HEX_SIZ]) {
	unsigned siz;
	unsigned char buf[20];
	if (!EVP_DigestFinal_ex(ctx->ctx, buf, &siz))
		return false;
	assert(siz == 20); /* This should always be true */
	static const char *const hex = "0123456789abcdef";
	for (unsigned i = 0; i < 20; i++) {
		*digest++ = hex[(buf[i] >> 4) & 0xf];
		*digest++ = hex[buf[i] & 0xf];
	}
	return true;
}
