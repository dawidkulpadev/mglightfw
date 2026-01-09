//
// Created by dkulpa on 7.12.2025.
//

#include "Encryption.h"
#include <mbedtls/gcm.h>

#include <mbedtls/ecdh.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>

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

bool Encryption::encryptAESGCM(const std::string *in, uint8_t *iv, uint8_t *tag,
                               uint8_t *aad, std::string *out, uint8_t *key) {
    // AES-256-GCM
    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&g); return false;
    }

    if (mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, in->length(),
                                  iv, 12, aad, sizeof(aad),
                                  reinterpret_cast<const unsigned char *>(in->c_str()), (uint8_t*)out->data(), 16, tag) != 0) {
        mbedtls_gcm_free(&g);
        return false;
    }
    mbedtls_gcm_free(&g);

    return true;
}


bool Encryption::verifyCertificateMbedTLS(const uint8_t* data, size_t dataLen,
                                          const uint8_t* signature, size_t sigLen,
                                          const uint8_t* pubKeyRaw, size_t pubKeyLen){
    // Przyjmujemy: ECDSA P-256, surowy podpis R||S (64 bajty) i klucz publiczny w formacie
    //  - 65 bajtów: [0x04 | X(32) | Y(32)]  (nieskompresowany)
    //  - 64 bajty:  [X(32) | Y(32)]         (bez prefiksu – dodamy sami)

    constexpr size_t P256_RS_SIG_LEN        = 64;
    constexpr size_t P256_UNCOMP_PUBKEY_LEN = 65;
    constexpr size_t P256_COORD_LEN         = 32;

    int ret = 0;

    if (data == nullptr || dataLen == 0 ||
        signature == nullptr || sigLen != P256_RS_SIG_LEN ||
        pubKeyRaw == nullptr ||
        (pubKeyLen != P256_UNCOMP_PUBKEY_LEN && pubKeyLen != 2 * P256_COORD_LEN)) {
        return false;
    }

    mbedtls_ecp_group group;
    mbedtls_ecp_point Q;
    mbedtls_mpi r, s;
    unsigned char hash[32] = {0};
    unsigned char pubKeyBuf[P256_UNCOMP_PUBKEY_LEN];

    mbedtls_ecp_group_init(&group);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    ret = mbedtls_sha256_ret(data, dataLen, hash, 0 /* is224 = 0 => SHA-256 */);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto cleanup;
    }

    if (pubKeyLen == P256_UNCOMP_PUBKEY_LEN) {
        if (pubKeyRaw[0] != 0x04) {
            ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            goto cleanup;
        }
        memcpy(pubKeyBuf, pubKeyRaw, P256_UNCOMP_PUBKEY_LEN);
    } else {
        pubKeyBuf[0] = 0x04;
        memcpy(pubKeyBuf + 1, pubKeyRaw, 2 * P256_COORD_LEN);
    }

    ret = mbedtls_ecp_point_read_binary(&group, &Q, pubKeyBuf, P256_UNCOMP_PUBKEY_LEN);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_check_pubkey(&group, &Q);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&r, signature, P256_COORD_LEN);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&s, signature + P256_COORD_LEN, P256_COORD_LEN);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecdsa_verify(&group, hash, sizeof(hash), &Q, &r, &s);

    cleanup:
    mbedtls_platform_zeroize(hash, sizeof(hash));
    mbedtls_ecp_group_free(&group);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret == 0);
}

// data - surowe dane do podpisania
// dataLen - długość tych danych
// privKeyD - klucz prywatny ECDSA P-256 R||S o długości 64 bajtów
// privKeyDLen - długość klucza. Tutaj przyjmiemy że zawsze będzie 64 bajty
// signatureOut - musi być tablicą 64 bajty
bool Encryption::signDataMbedTLS_P256_RS(const uint8_t* data, size_t dataLen,
                                         const uint8_t* privKeyD, size_t privKeyDLen,
                                         uint8_t* signatureOut, size_t sigOutLen) {
    // Podpis: ECDSA P-256, output raw R||S (64B)
    // Klucz prywatny: skalar d (32B) big-endian

    constexpr size_t P256_D_LEN      = 32;
    constexpr size_t P256_RS_SIG_LEN = 64;

    int ret = 0;

    if (data == nullptr || dataLen == 0 ||
        privKeyD == nullptr || privKeyDLen != P256_D_LEN ||
        signatureOut == nullptr || sigOutLen != P256_RS_SIG_LEN) {
        return false;
    }

    unsigned char hash[32] = {0};

    mbedtls_ecp_group group;
    mbedtls_mpi d, r, s;

    mbedtls_ecp_group_init(&group);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    ret = mbedtls_sha256_ret(data, dataLen, hash, 0 /* SHA-256 */);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&d, privKeyD, P256_D_LEN);
    if (ret != 0) {
        goto cleanup;
    }

    if (mbedtls_mpi_cmp_int(&d, 0) == 0 || mbedtls_mpi_cmp_mpi(&d, &group.N) >= 0) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    ret = mbedtls_ecdsa_sign_det(&group, &r, &s, &d, hash, sizeof(hash), MBEDTLS_MD_SHA256);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&r, signatureOut, 32);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&s, signatureOut + 32, 32);
    if (ret != 0) {
        goto cleanup;
    }

    cleanup:
    mbedtls_platform_zeroize(hash, sizeof(hash));
    mbedtls_ecp_group_free(&group);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret == 0);
}

std::string Encryption::base64Encode(uint8_t *data, size_t dlen) {
    size_t olen;
    mbedtls_base64_encode(nullptr, 0, &olen, data, dlen);

    std::string out;
    out.resize(olen);
    mbedtls_base64_encode((unsigned char *) out.data(), olen, &olen, data, dlen);

    return out;
}

size_t Encryption::base64Decode(const std::string &in, uint8_t *out, size_t outLen) {
    size_t rlen;
    mbedtls_base64_decode(out, outLen, &rlen,
                          reinterpret_cast<const unsigned char *>(in.c_str()), in.size());

    return rlen;
}
