//
// Created by dkulpa on 17.08.2025.
//

#include "BLELNBase.h"

const char* BLELNBase::SERVICE_UUID   = "11111111-1111-1111-1111-111111111111";
const char* BLELNBase::KEYEX_TX_UUID  = "11111111-1111-1111-1111-111111111112";
const char* BLELNBase::KEYEX_RX_UUID  = "11111111-1111-1111-1111-111111111113";
const char* BLELNBase::DATA_TX_UUID   = "11111111-1111-1111-1111-111111111114";
const char* BLELNBase::DATA_RX_UUID   = "11111111-1111-1111-1111-111111111115";

bool BLELNBase::rngInitialised=false;
mbedtls_entropy_context BLELNBase::entropy;
mbedtls_ctr_drbg_context BLELNBase::ctr_drbg;

void BLELNBase::hkdf_sha256(const uint8_t* salt, size_t salt_len,
                        const uint8_t* ikm, size_t ikm_len,
                        const uint8_t* info, size_t info_len,
                        uint8_t* okm, size_t okm_len) {

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

void BLELNBase::load_or_init_psk(Preferences *prefs, uint8_t *g_psk_salt, uint32_t *g_epoch) {
    prefs->begin("sec", false);
    size_t have = prefs->getBytesLength("salt");
    if (have != 32) {
        random_bytes(g_psk_salt, 32);
        *g_epoch = 1;
        prefs->putBytes("salt", g_psk_salt, 32);
        prefs->putULong("epoch", *g_epoch);
    } else {
        prefs->getBytes("salt", g_psk_salt, 32);
        *g_epoch = prefs->getULong("epoch", 1);
        if (*g_epoch == 0) *g_epoch = 1;
    }
    prefs->end();
}

void BLELNBase::random_bytes(uint8_t *out, size_t len) {
    esp_fill_random(out, len);
}

void BLELNBase::rotate_psk(Preferences *prefs, uint8_t *g_psk_salt, uint32_t * g_epoch) {
    prefs->begin("sec", false);
    random_bytes(g_psk_salt, 32);
    *g_epoch = prefs->getULong("epoch", 1) + 1;
    prefs->putBytes("salt", g_psk_salt, 32);
    prefs->putULong("epoch", *g_epoch);
    prefs->end();
    Serial.printf("[PSK] Rotated. New epoch=%lu\n", (unsigned long)g_epoch);
}

void BLELNBase::rng_init() {
    if(!rngInitialised) {
        Serial.println("Rng init");
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        const char *pers = "srv-ecdh";
        mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *) pers, strlen(pers));
        rngInitialised = true;
    }
}

bool BLELNBase::ecdh_gen(uint8_t *pub65, mbedtls_ecp_group &g, mbedtls_mpi &d, mbedtls_ecp_point &Q) {
    mbedtls_ecp_group_init(&g); mbedtls_mpi_init(&d); mbedtls_ecp_point_init(&Q);
    if(mbedtls_ecp_group_load(&g, MBEDTLS_ECP_DP_SECP256R1)!=0)
        return false;

    if(mbedtls_ecp_gen_keypair(&g,&d,&Q,mbedtls_ctr_drbg_random,&ctr_drbg)!=0)
        return false;

    size_t olen=0;
    if(mbedtls_ecp_point_write_binary(&g,&Q,MBEDTLS_ECP_PF_UNCOMPRESSED,&olen,pub65,65)!=0)
        return false;

    return (olen==65 && pub65[0]==0x04);
}

bool BLELNBase::ecdh_shared(const mbedtls_ecp_group &g, const mbedtls_mpi &d, const uint8_t *pub65, uint8_t *out) {
    mbedtls_ecp_point P; mbedtls_ecp_point_init(&P);
    if(mbedtls_ecp_point_read_binary(&g,&P,pub65,65)!=0){
        mbedtls_ecp_point_free(&P);
        return false;
    }
    mbedtls_mpi sh; mbedtls_mpi_init(&sh);
    if(mbedtls_ecdh_compute_shared((mbedtls_ecp_group*)&g,&sh,&P,(mbedtls_mpi*)&d,mbedtls_ctr_drbg_random,&ctr_drbg)!=0){
        mbedtls_ecp_point_free(&P); mbedtls_mpi_free(&sh);
        return false;
    }
    bool ok = (mbedtls_mpi_write_binary(&sh,out,32)==0);

    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_free(&sh);
    return ok;
}

bool BLELNBase::gcm_encrypt(const uint8_t *key, const uint8_t *plain, size_t plen, const uint8_t *nonce12,
                            const uint8_t *aad, size_t aadLen, std::string &out, uint8_t *tag) {
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    if(mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256)!=0){
        mbedtls_gcm_free(&g);
        return false;
    }
    out.resize(plen);
    int rc = mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, plen, nonce12, 12, aad, aadLen, plain, (uint8_t*)out.data(), 16, tag);
    mbedtls_gcm_free(&g);
    return rc==0;
}
