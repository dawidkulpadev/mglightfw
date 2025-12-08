//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_BLELNSESSIONENC_H
#define MGLIGHTFW_BLELNSESSIONENC_H

#include <Arduino.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

class BLELNSessionEnc {
public:
    bool makeMyKeys(); // Initialize with new server keys
    bool deriveFriendsKey(const uint8_t* clientPub65,
                          const uint8_t* clientNonce12, uint8_t *g_psk_salt, uint32_t g_epoch);
    bool decryptAESGCM(const uint8_t* in, size_t inLen, std::string &out);
    bool encryptAESGCM(const std::string &in, std::string &out);

    uint16_t getSessionId();

    uint8_t* getMyPub();
    uint8_t* getMyNonce();
private:
    // Connection encryption
    uint16_t sid = 0;
    uint32_t epoch = 0;

    // My data
    mbedtls_ecp_group grp{};
    mbedtls_mpi d{};             // my private key

    uint8_t sessKey_m2f[32]{};   // AES-256-GCM key
    uint32_t myLastCtr = 0;     // anty-replay
    uint8_t myNonce[12]{};
    uint8_t myPub[65];           // my public key

    // Friend data
    uint8_t sessKey_f2m[32]{};   // AES-256-GCM key
    uint32_t friendsLastCtr = 0;    // anty-replay
    uint8_t friendsNonce[12]{};
};


#endif //MGLIGHTFW_BLELNSESSIONENC_H
