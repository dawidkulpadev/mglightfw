//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_BLELNCONNCTX_H
#define MGLIGHTFW_BLELNCONNCTX_H

#include <Arduino.h>
#include "BLELNSessionEnc.h"


class BLELNConnCtx {
public:
    enum class State {New, Initialised, WaitingForKey, WaitingForCert, ChallengeResponseCli ,ChallengeResponseSer, Authorised, AuthFailed};
    explicit BLELNConnCtx(uint16_t handle);
    ~BLELNConnCtx();

    uint16_t getHandle() const;
    void setState(State state);
    State getState();

    bool makeSessionKey();

    BLELNSessionEnc* getSessionEnc();
private:
    uint16_t h = 0;
    State s;

    BLELNSessionEnc bse;
};


#endif //MGLIGHTFW_BLELNCONNCTX_H
