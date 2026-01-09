//
// Created by dkulpa on 5.01.2026.
//

#include "BLELNAuthentication.h"
#include "Encryption.h"
#include "SuperString.h"

bool BLELNAuthentication::loadCert(Preferences *prefs) {
    prefs->getBytes("PCSign", certSign, 64);
    prefs->getBytes("ManuPub", manuPubKey, 64);
    prefs->getBytes("DevPriv", myPrivateKey, 64);
    prefs->getBytes("DevPub", myPublicKey, 64);

    return true;
}

std::string BLELNAuthentication::getSignedCert() {
    // Cert:
    // product generation - 1 byte
    // devices id (mac) 6 bytes
    // devices public key 64 bytes
    // sign 64 bytes (base64 encoded)
    std::string out;
    uint64_t mac= ESP.getEfuseMac();

    out.append("2;");
    out.append(Encryption::base64Encode((uint8_t*)&mac, 6));
    out.append(";");
    out.append(Encryption::base64Encode(myPublicKey, 64));
    out.append(",");
    out.append(Encryption::base64Encode(certSign, 64));

    return out;
}

void BLELNAuthentication::signData(std::string &d, uint8_t *out) {
    Encryption::signDataMbedTLS_P256_RS((const uint8_t*)d.data(), d.size(), myPrivateKey, 64, out, 64);
}

bool BLELNAuthentication::verifyCert(const std::string &cert, const std::string &sign) {
    uint8_t signRaw[64];
    Encryption::base64Decode(sign, signRaw, 64);
    bool r= Encryption::verifyCertificateMbedTLS(reinterpret_cast<const uint8_t *>(cert.data()), cert.length(),
                                         signRaw, 64, manuPubKey, 64);


    return r;
}
