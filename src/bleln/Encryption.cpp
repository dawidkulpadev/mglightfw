//
// Created by dkulpa on 7.12.2025.
//

#include "Encryption.h"

bool Encryption::rngInitialised=false;
mbedtls_entropy_context Encryption::entropy;
mbedtls_ctr_drbg_context Encryption::ctr_drbg;

void Encryption::randomizer_init() {
    if(!rngInitialised) {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        const char *pers = "srv-ecdh";
        mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *) pers, strlen(pers));
        rngInitialised = true;
    }
}

void Encryption::random_bytes(uint8_t *out, size_t len) {
    randomizer_init();
    esp_fill_random(out, len);
}

void
Encryption::hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len, const uint8_t *info,
                        size_t info_len, uint8_t *okm, size_t okm_len) {
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    uint8_t prk[32];
    mbedtls_md_context_t ctx; mbedtls_md_init(&ctx); mbedtls_md_setup(&ctx, md, 1);
    mbedtls_md_hmac_starts(&ctx, salt, salt_len);
    mbedtls_md_hmac_update(&ctx, ikm, ikm_len);
    mbedtls_md_hmac_finish(&ctx, prk);

    uint8_t T[32]; size_t Tlen=0, out=0; uint8_t cnt=1;
    while (out < okm_len) {
        mbedtls_md_hmac_starts(&ctx, prk, 32);
        if (Tlen) mbedtls_md_hmac_update(&ctx, T, Tlen);
        mbedtls_md_hmac_update(&ctx, info, info_len);
        mbedtls_md_hmac_update(&ctx, &cnt, 1);
        mbedtls_md_hmac_finish(&ctx, T);
        size_t cpy = min((size_t)32, okm_len - out);
        memcpy(okm + out, T, cpy);
        out += cpy; Tlen = 32; cnt++;
    }
    mbedtls_md_free(&ctx);
}

mbedtls_entropy_context *Encryption::getEntropy() {
    return &entropy;
}
