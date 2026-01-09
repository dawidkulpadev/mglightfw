//
// Created by dkulpa on 7.12.2025.
//

#include "BLELNConnCtx.h"


BLELNConnCtx::BLELNConnCtx(uint16_t handle) {
    s= State::New;
    h = handle;
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

