#include <shv/sha1.h>
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

bool sha1_digest(sha1ctx_t ctx, uint8_t digest[SHA1_SIZ]) {
	unsigned siz;
	if (!EVP_DigestFinal_ex(ctx->ctx, digest, &siz))
		return false;
	assert(siz == 20); /* This should always be true */
	return true;
}
