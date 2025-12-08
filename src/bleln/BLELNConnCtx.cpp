//
// Created by dkulpa on 7.12.2025.
//

#include "BLELNConnCtx.h"
#include "Encryption.h"


BLELNConnCtx::BLELNConnCtx(uint16_t handle) {
    s= State::Initialised;
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
    return bse.makeMyKeys();
}

BLELNSessionEnc *BLELNConnCtx::getSessionEnc() {
    return &bse;
}

