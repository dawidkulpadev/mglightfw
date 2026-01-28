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

#ifndef MGLIGHTFW_BLELNBASE_H
#define MGLIGHTFW_BLELNBASE_H

#define BLELN_TEST_NONCE_LEN        48
#define BLELN_DEV_PUB_KEY_LEN       64
#define BLELN_DEV_PRIV_KEY_LEN      32
#define BLELN_MANU_PUB_KEY_LEN      64
#define BLELN_DEV_SIGN_LEN          64
#define BLELN_MANU_SIGN_LEN         64
#define BLELN_NONCE_SIGN_LEN        64

#define BLELN_MSG_TITLE_CERT                                "$CERT"
#define BLELN_MSG_TITLE_CHALLENGE_RESPONSE_NONCE            "$CHRN"
#define BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW_AND_NONCE   "$CHRAN"
#define BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW             "$CHRA"
#define BLELN_MSG_TITLE_AUTH_OK                             "$AUOK"


#include "Arduino.h"


enum blen_wroker_actions {
    BLELN_WORKER_ACTION_REGISTER_CONNECTION,
    BLELN_WORKER_ACTION_DELETE_CONNECTION,
    BLELN_WORKER_ACTION_PROCESS_SUBSCRIPTION,
    BLELN_WORKER_ACTION_PROCESS_DATA_RX,
    BLELN_WORKER_ACTION_PROCESS_KEY_RX,
    BLELN_WORKER_ACTION_SEND_MESSAGE
};

struct BLELNWorkerAction {
    uint16_t connH;
    uint8_t type;
    size_t dlen;
    uint8_t *d;
};



class BLELNBase {
public:
    static const char* SERVICE_UUID;
    static const char* KEY_TO_CLI_UUID;
    static const char* KEY_TO_SER_UUID;
    static const char* DATA_TO_CLI_UUID;
    static const char* DATA_TO_SER_UUID;

    static void bytes_to_hex(const uint8_t *src, size_t src_len);
};


#endif //MGLIGHTFW_BLELNBASE_H
