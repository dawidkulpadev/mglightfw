//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_ENCRYPTION_H
#define MGLIGHTFW_ENCRYPTION_H

#include <Arduino.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>

class Encryption {
public:
    static void randomizer_init();
    static void random_bytes(uint8_t* out, size_t len);

    static void hkdf_sha256(const uint8_t* salt, size_t salt_len,
                            const uint8_t* ikm, size_t ikm_len,
                            const uint8_t* info, size_t info_len,
                            uint8_t* okm, size_t okm_len);

    static bool ecdh_gen(uint8_t *pub65, mbedtls_ecp_group &g, mbedtls_mpi &d);
    static bool ecdh_shared(const mbedtls_ecp_group &g, const mbedtls_mpi &d, const uint8_t *pub65, uint8_t *out);

    static std::string base64Encode(uint8_t *data, size_t dlen);
    static size_t base64Decode(const std::string &in, uint8_t *out, size_t outLen);

    static bool decryptAESGCM(const uint8_t* ct, size_t ctLen, const uint8_t *iv, const uint8_t *tag,
                              uint8_t *aad, std::string *out, uint8_t *key);
    static bool encryptAESGCM(const std::string *in, uint8_t *iv, uint8_t *tag,
                              uint8_t *aad, std::string *out, uint8_t *key);

    static bool signDataMbedTLS_P256_RS(const uint8_t* data, size_t dataLen,
                                             const uint8_t* privKeyD, size_t privKeyDLen,
                                             uint8_t* signatureOut, size_t sigOutLen);
    static bool verifyCertificateMbedTLS(const uint8_t* data, size_t dataLen, const uint8_t* signature, size_t sigLen,
                                  const uint8_t* pubKeyRaw, size_t pubKeyLen);

private:
    static bool rngInitialised;
    static mbedtls_entropy_context entropy;
    static mbedtls_ctr_drbg_context ctr_drbg;
};


#endif //MGLIGHTFW_ENCRYPTION_H
