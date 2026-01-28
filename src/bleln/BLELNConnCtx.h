/**
    MioGiapicco Light Firmware - Firmware for Light Device of MioGiapicco system
    Copyright (C) 2026  Dawid Kulpa

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Please feel free to contact me at any time by email <dawidkulpadev@gmail.com>
*/

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
    void generateTestNonce();
    uint8_t* getTestNonce();
    std::string getTestNonceBase64();
    bool verifyChallengeResponseAnswer(uint8_t *nonceSign);

    bool makeSessionKey();

    BLELNSessionEnc* getSessionEnc();

    unsigned long getTimeOfLife() const;
private:
    unsigned long birthTime;

    uint16_t h = 0;
    State s;

    uint8_t pubKey[BLELN_DEV_PUB_KEY_LEN]; // Public key of this other device i'm connecting with
    uint8_t mac[6]; // and its mac address
    uint8_t testNonce[BLELN_TEST_NONCE_LEN];

    BLELNSessionEnc bse;
};


#endif //MGLIGHTFW_BLELNCONNCTX_H
