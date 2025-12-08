//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_ENCRYPTION_H
#define MGLIGHTFW_ENCRYPTION_H

#include <Arduino.h>
#include <mbedtls/gcm.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>

class Encryption {
public:
    static void randomizer_init();
    static void random_bytes(uint8_t* out, size_t len);

    static void hkdf_sha256(const uint8_t* salt, size_t salt_len,
                            const uint8_t* ikm, size_t ikm_len,
                            const uint8_t* info, size_t info_len,
                            uint8_t* okm, size_t okm_len);

    static mbedtls_entropy_context* getEntropy();

private:
    static bool rngInitialised;
    static mbedtls_entropy_context entropy;
    static mbedtls_ctr_drbg_context ctr_drbg;
};


#endif //MGLIGHTFW_ENCRYPTION_H
