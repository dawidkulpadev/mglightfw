//
// Created by dkulpa on 7.12.2025.
//

#include "BLELNConnCtx.h"


BLELNConnCtx::BLELNConnCtx(uint16_t handle) {
    s= State::New;
    h = handle;
    birthTime= millis();
}

BLELNConnCtx::~BLELNConnCtx() {

}

uint16_t BLELNConnCtx::getHandle() const {
    return h;
}

void BLELNConnCtx::setState(BLELNConnCtx::State state) {
    s= state;
}

BLELNConnCtx::State BLELNConnCtx::getState() {
    return s;
}

bool BLELNConnCtx::makeSessionKey() {
    if(s==State::New) {
        if(bse.makeMyKeys()){
            s= State::Initialised;
            return true;
        }
    }

    return false;
}

BLELNSessionEnc *BLELNConnCtx::getSessionEnc() {
    return &bse;
}

void BLELNConnCtx::setCertData(uint8_t *macAddress, uint8_t *publicKey) {
    memcpy(mac, macAddress, 6);
    memcpy(pubKey, publicKey, BLELN_DEV_PUB_KEY_LEN);
}

void BLELNConnCtx::generateTestNonce() {
    Encryption::random_bytes(testNonce, BLELN_TEST_NONCE_LEN);
}


bool BLELNConnCtx::verifyChallengeResponseAnswer(uint8_t *nonceSign) {
    return Encryption::verifySign_ECDSA_P256(testNonce, BLELN_TEST_NONCE_LEN, nonceSign,
                                             BLELN_DEV_SIGN_LEN, pubKey, BLELN_DEV_PUB_KEY_LEN);
}

uint8_t *BLELNConnCtx::getTestNonce() {
    return testNonce;
}

std::string BLELNConnCtx::getTestNonceBase64() {
    return Encryption::base64Encode(testNonce, BLELN_TEST_NONCE_LEN);
}

unsigned long BLELNConnCtx::getTimeOfLife() const {
    return millis()-birthTime;
}
