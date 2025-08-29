//
// Created by dkulpa on 17.08.2025.
//

#include "BLELNClient.h"

void BLELNClient::start() {
    BLELNBase::rng_init();
    NimBLEDevice::init("ESP32C3 Client AutoSalt");
    NimBLEDevice::setSecurityAuth(true,true,true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setMTU(247);
}

void BLELNClient::stop() {
    if(client!= nullptr){
        client->disconnect();
        NimBLEDevice::deleteClient(client);
    }
    NimBLEDevice::deinit(true);
}

void BLELNClient::startServerSearch(uint32_t durationMs, const std::function<void(bool)>& onResult) {
    scanning = true;
    onScanResult= onResult;
    auto* scan=NimBLEDevice::getScan();
    scan->setScanCallbacks(this, false);
    scan->setActiveScan(true);
    scan->start(durationMs, false, false);
}


void BLELNClient::onDiscovered(const NimBLEAdvertisedDevice *advertisedDevice) {
    Serial.println(advertisedDevice->toString().c_str());
}

void BLELNClient::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(BLELNBase::SERVICE_UUID))) {
        scanning = false;
        NimBLEDevice::getScan()->stop();
        Serial.println("Setting callbacks");
        client = NimBLEDevice::createClient();
        client->setClientCallbacks(this, false);
        if (!client->connect(advertisedDevice, true, true, true)) {
            Serial.println("connect fail");
            NimBLEDevice::getScan()->start(0,false,false);
            return;
        }
        client->secureConnection();
        Serial.println("connected");
        if(onScanResult){
            onScanResult(true);
        }
    }
}

void BLELNClient::onScanEnd(const NimBLEScanResults &scanResults, int reason) {
    scanning = false;
    if(onScanResult){
        onScanResult(false);
    }
}

bool BLELNClient::discover() {
    auto* s = client->getService(BLELNBase::SERVICE_UUID); if(!s) return false;
    svc=s;
    chKeyExTx = s->getCharacteristic(BLELNBase::KEYEX_TX_UUID);
    chKeyExRx = s->getCharacteristic(BLELNBase::KEYEX_RX_UUID);
    chDataTx  = s->getCharacteristic(BLELNBase::DATA_TX_UUID);
    chDataRx  = s->getCharacteristic(BLELNBase::DATA_RX_UUID);

    chKeyExTx->subscribe(true, [this](NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
        this->onKeyExNotifyClb(pBLERemoteCharacteristic, pData, length, isNotify);
    });
    chDataTx->subscribe(true, [this](NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
        this->onServerResponse(pBLERemoteCharacteristic, pData, length, isNotify);
    });

    return chKeyExTx && chKeyExRx && chDataTx && chDataRx;
}

bool BLELNClient::handshake() {
    // KEYEX_TX: [ver=1][epoch:4][salt:32][srvPub:65][srvNonce:12]
    uint32_t t0 = millis();

    // Max wait: 5s
    Serial.println("Waiting for g_keyexReady");
    while (!g_keyexReady && millis() - t0 < 5000) {
        delay(10);
    }
    Serial.println("KeyEx received");

    if (!g_keyexReady) {
        Serial.println("[HX] timeout waiting KEYEX_TX notify");
        return false;
    }

    const std::string &v = g_keyexPayload;

    if (v.size()!=1+4+32+65+12 || (uint8_t)v[0]!=1) { Serial.printf("[HX] bad keyex len=%u\n",(unsigned)v.size()); return false; }
    memcpy(&s_epoch,  &v[1], 4);
    memcpy(s_salt,    &v[1+4], 32);
    memcpy(s_srvPub,  &v[1+4+32], 65);
    memcpy(s_srvNonce,&v[1+4+32+65], 12);

    mbedtls_ecp_group g; mbedtls_mpi d; mbedtls_ecp_point Q;
    if(!BLELNBase::ecdh_gen(s_cliPub,g,d,Q)){
        Serial.println("[HX] ecdh_gen fail"); return false;
    }
    BLELNBase::random_bytes(s_cliNonce,12);

    // Wyślij [ver=1][cliPub:65][cliNonce:12]
    std::string tx; tx.push_back(1);
    tx.append((const char*)s_cliPub,65);
    tx.append((const char*)s_cliNonce,12);


    if(!chKeyExRx->writeValue(tx,true)){
        Serial.println("[HX] write fail"); return false;
    }

    // ECDH -> shared
    uint8_t ss[32];
    if(!BLELNBase::ecdh_shared(g,d,s_srvPub,ss)){
        Serial.println("[HX] shared fail"); return false;
    }

    // HKDF salt = salt || epoch(LE)
    uint8_t salt[32+4];
    memcpy(salt,s_salt,32);
    salt[32]=(uint8_t)(s_epoch&0xFF);
    salt[33]=(uint8_t)((s_epoch>>8)&0xFF);
    salt[34]=(uint8_t)((s_epoch>>16)&0xFF);
    salt[35]=(uint8_t)((s_epoch>>24)&0xFF);

    const char infoHdr_s2c[]="BLEv1|sessKey_s2c";
    uint8_t info_s2c[sizeof(infoHdr_s2c) - 1 + 65 + 65 + 12 + 12], *p_s2c=info_s2c;
    memcpy(p_s2c, infoHdr_s2c, sizeof(infoHdr_s2c) - 1);
    p_s2c+= sizeof(infoHdr_s2c) - 1;
    memcpy(p_s2c, s_srvPub, 65); p_s2c+=65;
    memcpy(p_s2c, s_cliPub, 65); p_s2c+=65;
    memcpy(p_s2c, s_srvNonce, 12); p_s2c+=12;
    memcpy(p_s2c, s_cliNonce, 12); p_s2c+=12;
    BLELNBase::hkdf_sha256(salt, sizeof(salt), ss, sizeof(ss), info_s2c, (size_t)(p_s2c - info_s2c), s_sessKey_s2c, 32);

    const char infoHdr_c2s[]="BLEv1|sessKey_c2s";
    uint8_t info_c2s[sizeof(infoHdr_c2s) - 1 + 65 + 65 + 12 + 12], *p_c2s=info_c2s;
    memcpy(p_c2s, infoHdr_c2s, sizeof(infoHdr_c2s) - 1);
    p_c2s+= sizeof(infoHdr_c2s) - 1;
    memcpy(p_c2s, s_srvPub, 65); p_c2s+=65;
    memcpy(p_c2s, s_cliPub, 65); p_c2s+=65;
    memcpy(p_c2s, s_srvNonce, 12); p_c2s+=12;
    memcpy(p_c2s, s_cliNonce, 12); p_c2s+=12;
    BLELNBase::hkdf_sha256(salt, sizeof(salt), ss, sizeof(ss), info_c2s, (size_t)(p_c2s - info_c2s), s_sessKey_c2s, 32);

    uint8_t sidBuf[2];
    const char sidInfo[] = "BLEv1|sid";
    BLELNBase::hkdf_sha256(salt, sizeof(salt),
                ss, sizeof(ss),
                (const uint8_t*)sidInfo, sizeof(sidInfo)-1,
                sidBuf, sizeof(sidBuf));
    s_sid = ((uint16_t)sidBuf[0] << 8) | sidBuf[1];

    s_ctr_s2c=0;
    s_ctr_c2s=0;
    mbedtls_ecp_point_free(&Q); mbedtls_mpi_free(&d); mbedtls_ecp_group_free(&g);
    return true;
}

bool BLELNClient::sendEncrypted(const std::string &msg, std::function<void(std::string)> onServerResponse) {
    onMsgRx= onServerResponse;

    const char aadhdr[]="DATAv1";
    uint8_t aad[sizeof(aadhdr)-1+2+4], *a=aad;
    memcpy(a,aadhdr,sizeof(aadhdr)-1); a+=sizeof(aadhdr)-1;
    *a++= (uint8_t)(s_sid&0xFF);
    *a++= (uint8_t)(s_sid>>8);
    *a++= (uint8_t)(s_epoch&0xFF);
    *a++= (uint8_t)((s_epoch>>8)&0xFF);
    *a++= (uint8_t)((s_epoch>>16)&0xFF);
    *a=   (uint8_t)((s_epoch>>24)&0xFF);

    s_ctr_c2s++;
    uint8_t nonce[12];
    BLELNBase::random_bytes(nonce,12);
    std::string ct; uint8_t tag[16];

    if(!BLELNBase::gcm_encrypt(s_sessKey_c2s,(const uint8_t*)msg.data(),msg.size(),nonce,aad,sizeof(aad),ct,tag)){
        Serial.println("[GCM] fail");
        return false;
    }

    std::string pkt; pkt.resize(4);
    pkt[0]=(uint8_t)((s_ctr_c2s>>24)&0xFF);
    pkt[1]=(uint8_t)((s_ctr_c2s>>16)&0xFF);
    pkt[2]=(uint8_t)((s_ctr_c2s>>8)&0xFF);
    pkt[3]=(uint8_t)(s_ctr_c2s&0xFF);
    pkt.append((const char*)nonce,12);
    pkt.append(ct);
    pkt.append((const char*)tag,16);

    return chDataRx->writeValue(pkt,false);
}

bool BLELNClient::isConnected() {
    return (client!= nullptr) && (client->isConnected());
}

bool BLELNClient::hasDiscoveredClient() {
    return svc!= nullptr;
}

void BLELNClient::onPassKeyEntry(NimBLEConnInfo &connInfo) {
    g_keyexReady = false;
    NimBLEDevice::injectPassKey(connInfo, 123456);
}

void BLELNClient::onKeyExNotifyClb(NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                              bool isNotify) {
    g_keyexPayload.assign((const char*)pData, length);
    g_keyexReady = true;
}

void BLELNClient::onServerResponse(NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                                   bool isNotify) {
    if (length < 4+12+16) return;

    const uint8_t* ctrBE = pData;
    const uint8_t* nonce = pData+4;
    const uint8_t* ct    = pData+4+12;
    size_t ctLen = length - (4+12+16);
    const uint8_t* tag   = pData + (length - 16);

    uint32_t ctr = (uint32_t)ctrBE[0]<<24 | (uint32_t)ctrBE[1]<<16 |
                   (uint32_t)ctrBE[2]<<8  | (uint32_t)ctrBE[3];
    if (ctr <= s_ctr_s2c) return; // anty-replay

    // AAD: "DATAv1"|sid(BE)|epoch(LE)
    const char aadhdr[]="DATAv1";
    uint8_t aad[sizeof(aadhdr)-1 + 2 + 4], *a=aad;
    memcpy(a,aadhdr,sizeof(aadhdr)-1); a+=sizeof(aadhdr)-1;
    *a++ = (uint8_t)(s_sid >> 8);
    *a++ = (uint8_t)(s_sid & 0xFF);
    *a++ = (uint8_t)(s_epoch & 0xFF);
    *a++ = (uint8_t)((s_epoch >> 8) & 0xFF);
    *a++ = (uint8_t)((s_epoch >> 16) & 0xFF);
    *a++ = (uint8_t)((s_epoch >> 24) & 0xFF);

    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, s_sessKey_s2c, 256) != 0) { mbedtls_gcm_free(&g); return; }

    std::string plain; plain.resize(ctLen);
    int rc = mbedtls_gcm_auth_decrypt(&g, ctLen, nonce, 12, aad, sizeof(aad),
                                      tag, 16, ct, (uint8_t*)plain.data());
    mbedtls_gcm_free(&g);
    if (rc != 0) return;

    s_ctr_s2c = ctr;

    if(onMsgRx){
        onMsgRx(plain);
    }
}

bool BLELNClient::isScanning() const {
    return scanning;
}


