//
// Created by dkulpa on 7.12.2025.
//

#include "Encryption.h"
#include <mbedtls/gcm.h>

#include <mbedtls/ecdh.h>
#include <mbedtls/md.h>

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

bool Encryption::ecdh_gen(uint8_t *pub65, mbedtls_ecp_group &g, mbedtls_mpi &d) {
    mbedtls_ecp_point Q{};

    mbedtls_ecp_group_init(&g);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);
    if(mbedtls_ecp_group_load(&g, MBEDTLS_ECP_DP_SECP256R1)!=0)
        return false;

    if(mbedtls_ecp_gen_keypair(&g,&d,&Q,mbedtls_ctr_drbg_random,&ctr_drbg)!=0)
        return false;

    size_t olen=0;
    if(mbedtls_ecp_point_write_binary(&g,&Q,MBEDTLS_ECP_PF_UNCOMPRESSED,&olen,pub65,65)!=0)
        return false;

    return (olen==65 && pub65[0]==0x04);
}

bool Encryption::ecdh_shared(const mbedtls_ecp_group &g, const mbedtls_mpi &d, const uint8_t *pub65, uint8_t *out) {
    mbedtls_ecp_point P;
    mbedtls_ecp_point_init(&P);
    if(mbedtls_ecp_point_read_binary(&g,&P,pub65,65)!=0){
        mbedtls_ecp_point_free(&P);
        return false;
    }
    mbedtls_mpi sh;
    mbedtls_mpi_init(&sh);
    if(mbedtls_ecdh_compute_shared((mbedtls_ecp_group*)&g,&sh,&P,(mbedtls_mpi*)&d,mbedtls_ctr_drbg_random,&ctr_drbg)!=0){
        mbedtls_ecp_point_free(&P); mbedtls_mpi_free(&sh);
        return false;
    }
    bool ok = (mbedtls_mpi_write_binary(&sh,out,32)==0);

    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_free(&sh);
    return ok;
}

bool Encryption::decryptAESGCM(const uint8_t *ct, size_t ctLen, const uint8_t *iv, const uint8_t *tag,
                               uint8_t *aad, std::string *out, uint8_t *key) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    out->resize(ctLen);
    int rc = mbedtls_gcm_auth_decrypt(&gcm, ctLen,
                                      iv, 12,
                                      aad, sizeof(aad),
                                      tag, 16,
                                      ct, (unsigned char*)out->data());
    mbedtls_gcm_free(&gcm);
    if (rc != 0) {
        return false;
    }


    return true;
}

bool Encryption::encryptAESGCM(const std::string *in, const std::string *ct, uint8_t *iv, uint8_t *tag,
                               uint8_t *aad, std::string *out, uint8_t *key) {
    // AES-256-GCM
    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&g); return false;
    }

    if (mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, in->length(),
                                  iv, 12, aad, sizeof(aad),
                                  reinterpret_cast<const unsigned char *>(in->c_str()), (uint8_t*)ct->data(), 16, tag) != 0) {
        mbedtls_gcm_free(&g); return false;
    }
    mbedtls_gcm_free(&g);



    return true;
}
