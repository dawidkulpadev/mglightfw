//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_BLELNCONNCTX_H
#define MGLIGHTFW_BLELNCONNCTX_H

#include <Arduino.h>
#include "BLELNSessionEnc.h"
#include "BLELNBase.h"
#include "Encryption.h"


class BLELNConnCtx {
public:
    enum class State {New, Initialised, WaitingForKey, WaitingForCert, ChallengeResponseCli ,ChallengeResponseSer, Authorised, AuthFailed};
    explicit BLELNConnCtx(uint16_t handle);
    ~BLELNConnCtx();

    uint16_t getHandle() const;
    void setState(State state);
    State getState();

    void setCertData(uint8_t *macAddress, uint8_t *publicKey);
    void setTestNonce(uint8_t *nonce);
    bool verifyChallengeResponseAnswer(uint8_t *nonceSign);

    bool makeSessionKey();

    BLELNSessionEnc* getSessionEnc();
private:
    uint16_t h = 0;
    State s;

    uint8_t pubKey[BLELN_DEV_PUB_KEY_LEN]; // Public key of this other device i'm connecting with
    uint8_t mac[6]; // and its mac address
    uint8_t testNonce[BLELN_TEST_NONCE_LEN];

    BLELNSessionEnc bse;
};


#endif //MGLIGHTFW_BLELNCONNCTX_H
