//
// Created by dkulpa on 5.01.2026.
//

#include "BLELNAuthentication.h"
#include "Encryption.h"
#include "SuperString.h"

bool BLELNAuthentication::loadCert() {
    Preferences prefs;

    if(prefs.begin("cert", true)) {
        size_t r= prefs.getBytes("pc_sign", certSign, BLELN_MANU_SIGN_LEN);
        prefs.getBytes("manu_pub", manuPubKey, BLELN_MANU_PUB_KEY_LEN);
        prefs.getBytes("dev_priv", myPrivateKey, BLELN_DEV_PRIV_KEY_LEN);
        prefs.getBytes("dev_pub", myPublicKey, BLELN_DEV_PUB_KEY_LEN);
        prefs.end();
    } else {
        Serial.println("BLELNAuthentication - loadCert() - failed");
        return false;
    }

    return true;
}

std::string BLELNAuthentication::getSignedCert() {
    // # Cert:
    // product generation - as text
    // ;
    // devices mac 6 bytes  - base64
    // ;
    // devices public key 64 bytes - base64
    // # Sign:
    // ,
    // certSign - base64

    std::string out;
    uint64_t mac= ESP.getEfuseMac();

    out.append("2;");
    out.append(Encryption::base64Encode((uint8_t*)&mac, 6));
    out.append(";");
    out.append(Encryption::base64Encode(myPublicKey, BLELN_DEV_PUB_KEY_LEN));
    out.append(",");
    out.append(Encryption::base64Encode(certSign, BLELN_MANU_SIGN_LEN));

    return out;
}


bool BLELNAuthentication::verifyCert(const std::string &cert, const std::string &sign, uint8_t *genOut, uint8_t *macOut,
                                     int macOutLen, uint8_t *pubKeyOut, int pubKeyOutLen) {
    uint8_t signRaw[BLELN_MANU_SIGN_LEN];
    Encryption::base64Decode(sign, signRaw, BLELN_MANU_SIGN_LEN);

    bool r= Encryption::verifySign_ECDSA_P256(reinterpret_cast<const uint8_t *>(cert.data()), cert.length(),
                                              signRaw, BLELN_MANU_SIGN_LEN, manuPubKey, BLELN_MANU_PUB_KEY_LEN);

    if((macOutLen < 6) or (pubKeyOutLen < BLELN_DEV_PUB_KEY_LEN)){
        return false;
    }

    if(r){
        StringList certSplit= splitCsvRespectingQuotes(cert, ';');
        try {
            *genOut = std::stoi(certSplit[0], nullptr, 10);
        } catch (std::invalid_argument &e){
            return false;
        } catch (std::out_of_range &e) {
            return false;
        }

        if(Encryption::base64Decode(certSplit[1], macOut, macOutLen)==6){
            if(Encryption::base64Decode(certSplit[2], pubKeyOut, pubKeyOutLen)!=BLELN_DEV_PUB_KEY_LEN){
                return false;
            }
        } else {
            return false;
        }
    }

    return r;
}

void BLELNAuthentication::signData(std::string &d, uint8_t *out) {
    Encryption::signData_ECDSA_P256((const uint8_t *) d.data(), d.size(),
                                    myPrivateKey, BLELN_DEV_PRIV_KEY_LEN, out, BLELN_DEV_SIGN_LEN);
}

void BLELNAuthentication::signData(const uint8_t *d, size_t dlen, uint8_t *out) {
    Encryption::signData_ECDSA_P256(d, dlen,
                                    myPrivateKey, BLELN_DEV_PRIV_KEY_LEN, out, BLELN_DEV_SIGN_LEN);
}
