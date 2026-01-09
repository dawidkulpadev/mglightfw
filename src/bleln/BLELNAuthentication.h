//
// Created by dkulpa on 5.01.2026.
//

#ifndef MGLIGHTFW_BLELNAUTHENTICATION_H
#define MGLIGHTFW_BLELNAUTHENTICATION_H

#include "Arduino.h"
#include "Preferences.h"
#include "mbedtls/base64.h"

class BLELNAuthentication {
public:
    bool loadCert(Preferences *prefs);
    std::string getSignedCert();
    bool verifyCert(const std::string &cert, const std::string &sign);
    void signData(std::string &d, uint8_t *out);

private:
    uint8_t certSign[64];
    uint8_t manuPubKey[64];
    uint8_t myPrivateKey[64];
    uint8_t myPublicKey[64];

    uint8_t friendsPubKey[64];
    uint8_t friendsId[6];
};

#endif //MGLIGHTFW_BLELNAUTHENTICATION_H
