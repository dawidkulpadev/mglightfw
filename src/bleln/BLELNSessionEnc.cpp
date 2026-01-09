//
// Created by dkulpa on 7.12.2025.
//

#include "BLELNSessionEnc.h"
#include "Encryption.h"

bool BLELNSessionEnc::makeMyKeys() {
    if(!Encryption::ecdh_gen(myPub,grp,d)){
        Serial.println("[HX] ecdh_gen fail");
        return false;
    }
    Encryption::random_bytes(myNonce,12);

    return true;
}

bool BLELNSessionEnc::deriveFriendsKey(const uint8_t *clientPub65,
                                    const uint8_t *clientNonce12, uint8_t *g_psk_salt, uint32_t g_epoch) {
    // ECDH -> shared
    uint8_t ss[32];
    if(!Encryption::ecdh_shared(grp,d,myPub,ss)){
        return false;
    }

    // HKDF: salt = PSK_SALT || epoch (LE)
    uint8_t salt[32+4];
    memcpy(salt, g_psk_salt, 32);
    salt[32] = (uint8_t)(g_epoch & 0xFF);
    salt[33] = (uint8_t)((g_epoch >> 8) & 0xFF);
    salt[34] = (uint8_t)((g_epoch >> 16) & 0xFF);
    salt[35] = (uint8_t)((g_epoch >> 24) & 0xFF);

    // info = "BLEv1|sess" + srvPub + cliPub + srvNonce + cliNonce
    const char infoHdr_c2s[] = "BLEv1|sessKey_c2s";
    uint8_t info_c2s[ sizeof(infoHdr_c2s)-1 + 65 + 65 + 12 + 12 ];
    uint8_t* p_c2s = info_c2s;
    memcpy(p_c2s, infoHdr_c2s, sizeof(infoHdr_c2s)-1); p_c2s += sizeof(infoHdr_c2s)-1;
    memcpy(p_c2s, myPub, 65); p_c2s += 65;
    memcpy(p_c2s, clientPub65, 65); p_c2s += 65;
    memcpy(p_c2s, myNonce, 12); p_c2s += 12;
    memcpy(p_c2s, clientNonce12, 12); p_c2s += 12;
    auto infoLen_c2s = (size_t)(p_c2s - info_c2s);
    Encryption::hkdf_sha256(salt, sizeof(salt), ss, sizeof(ss), info_c2s, infoLen_c2s,
                            sessKey_f2m, 32);

    // info = "BLEv1|sess" + srvPub + cliPub + srvNonce + cliNonce
    const char infoHdr_s2c[] = "BLEv1|sessKey_s2c";
    uint8_t info_s2c[sizeof(infoHdr_s2c) - 1 + 65 + 65 + 12 + 12 ];
    uint8_t* p_s2c = info_s2c;
    memcpy(p_s2c, infoHdr_s2c, sizeof(infoHdr_s2c) - 1); p_s2c += sizeof(infoHdr_s2c) - 1;
    memcpy(p_s2c, myPub, 65); p_s2c += 65;
    memcpy(p_s2c, clientPub65, 65); p_s2c += 65;
    memcpy(p_s2c, myNonce, 12); p_s2c += 12;
    memcpy(p_s2c, clientNonce12, 12); p_s2c += 12;
    auto infoLen_s2c = (size_t)(p_s2c - info_s2c);
    Encryption::hkdf_sha256(salt, sizeof(salt), ss, sizeof(ss), info_s2c, infoLen_s2c,
                            sessKey_m2f, 32);

    uint8_t sidBuf[2];
    const char sidInfo[] = "BLEv1|sid";
    Encryption::hkdf_sha256(salt, sizeof(salt),
                            ss, sizeof(ss),
                            (const uint8_t*)sidInfo, sizeof(sidInfo)-1,
                            sidBuf, sizeof(sidBuf));
    sid = ((uint16_t)sidBuf[0] << 8) | sidBuf[1];


    myLastCtr = 0;
    friendsLastCtr = 0;
    epoch = g_epoch;

    return true;
}

bool BLELNSessionEnc::decryptMessage(const uint8_t *in, size_t inLen, std::string &out) {
    if (inLen < 4 + 12 + 16) {
        return false;
    }

    const uint8_t* p = in;
    uint32_t r_ctr = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]; // Received CTR
    p += 4;
    const uint8_t* iv = p;
    p += 12;
    size_t ctLen = inLen - (4 + 12 + 16);
    const uint8_t* ct = p;
    p += ctLen;
    const uint8_t* tag = p;

    if (r_ctr <= friendsLastCtr){
        return false;
    }

    // AAD = "DATAv1" + handle(LE) + epochUsed(LE)
    const char aadhdr[] = "DATAv1";
    uint8_t aad[ sizeof(aadhdr)-1 + 2 + 4 ];
    uint8_t* a = aad;
    memcpy(a, aadhdr, sizeof(aadhdr)-1); a += sizeof(aadhdr)-1;
    *a++ = (uint8_t)(sid >> 8);
    *a++ = (uint8_t)(sid & 0xFF);
    *a++ = (uint8_t)(epoch & 0xFF);
    *a++ = (uint8_t)((epoch >> 8) & 0xFF);
    *a++ = (uint8_t)((epoch >> 16) & 0xFF);
    *a = (uint8_t)((epoch >> 24) & 0xFF);

    if(!Encryption::decryptAESGCM(ct, ctLen, iv, tag, aad, &out, sessKey_f2m)){
        return false;
    }

    friendsLastCtr = r_ctr;
    return true;
}

bool BLELNSessionEnc::encryptMessage(const std::string &in, std::string &out) {
    // AAD = "DATAv1" | sid(BE) | epoch(LE)
    const char aadhdr[] = "DATAv1";
    uint8_t aad[sizeof(aadhdr)-1 + 2 + 4], *a=aad;
    memcpy(a,aadhdr,sizeof(aadhdr)-1);
    a+=sizeof(aadhdr)-1;
    *a++ = (uint8_t)(sid >> 8);
    *a++ = (uint8_t)(sid & 0xFF);
    *a++ = (uint8_t)(epoch & 0xFF);
    *a++ = (uint8_t)((epoch >> 8) & 0xFF);
    *a++ = (uint8_t)((epoch >> 16) & 0xFF);
    *a   = (uint8_t)((epoch >> 24) & 0xFF);

    myLastCtr++;
    uint8_t ctrBE[4] = {
            (uint8_t)((myLastCtr>>24)&0xFF), (uint8_t)((myLastCtr>>16)&0xFF),
            (uint8_t)((myLastCtr>>8)&0xFF),  (uint8_t)( myLastCtr&0xFF)
    };
    uint8_t iv[12];
    Encryption::random_bytes(iv, 12);

    std::string ct;
    ct.resize(in.length());
    uint8_t tag[16];

    if(!Encryption::encryptAESGCM(&in, iv, tag, aad, &ct, sessKey_m2f)){
        return false;
    }
    // Pakiet: [ctr:4][nonce:12][cipher...][tag:16]
    out.erase();
    out.append((char*)ctrBE,4);
    out.append((char*)iv,12);
    out.append(ct);
    out.append((char*)tag,16);

    return true;
}

uint8_t *BLELNSessionEnc::getMyPub() {
    return myPub;
}

uint8_t *BLELNSessionEnc::getMyNonce() {
    return myNonce;
}

uint16_t BLELNSessionEnc::getSessionId() {
    return sid;
}
