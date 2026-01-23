//
// Created by dkulpa on 5.01.2026.
//

#ifndef MGLIGHTFW_BLELNAUTHENTICATION_H
#define MGLIGHTFW_BLELNAUTHENTICATION_H

#include "Arduino.h"
#include "Preferences.h"
#include "mbedtls/base64.h"
#include "BLELNBase.h"

class BLELNAuthentication {
public:
    bool loadCert();
    std::string getSignedCert();
    bool verifyCert(const std::string &cert, const std::string &sign, uint8_t *genOut, uint8_t *macOut,
                    int macOutLen, uint8_t *pubKeyOut, int pubKeyOutLen);
    void signData(std::string &d, uint8_t *out);
    void signData(const uint8_t *d, size_t dlen, uint8_t *out);

private:
    uint8_t certSign[BLELN_MANU_SIGN_LEN];
    uint8_t manuPubKey[BLELN_MANU_PUB_KEY_LEN];
    uint8_t myPrivateKey[BLELN_DEV_PRIV_KEY_LEN];
    uint8_t myPublicKey[BLELN_DEV_PUB_KEY_LEN];
};

#endif //MGLIGHTFW_BLELNAUTHENTICATION_H
