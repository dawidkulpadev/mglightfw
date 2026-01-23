//
// Created by dkulpa on 7.12.2025.
//

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

class BLELNBase {
public:
    static const char* SERVICE_UUID;
    static const char* KEYEX_TX_UUID;
    static const char* KEYEX_RX_UUID;
    static const char* DATA_TX_UUID;
    static const char* DATA_RX_UUID;
};


#endif //MGLIGHTFW_BLELNBASE_H
