//
// Created by dkulpa on 17.08.2025.
//

#include "BLELNServer.h"
#include "BLELNBase.h"

void BLELNServer::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
    BLELNConnCtx* c = nullptr;

    if(!getConnContext(connInfo.getConnHandle(), &c)){
        Serial.println("Failed searching for context!");
        return;
    }

    if (c == nullptr) {
        Serial.println("Creating new ConnCtx");
        if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(100))!=pdTRUE){
            Serial.println("Failed locking semaphore! (create new client)!");
            return;
        }
        connCtxs.emplace_back(connInfo.getConnHandle());
        c = (connCtxs.end()-1).base();
        xSemaphoreGive(clisMtx);
        Serial.println(("New client handle: "+std::to_string(c->getHandle())).c_str());
    }
    if (!c->getEncData()->makeServerKeys(&BLELNBase::ctr_drbg)) {
        Serial.println("ECDH keygen fail");
        return;
    }


    NimBLEDevice::startAdvertising();
}

void BLELNServer::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) {
    int removeIdx = -1;

    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(100))!=pdTRUE){
        Serial.println("Failed locking semaphore! (onDisconnect)");
        return;
    }
    for (int i=0; i<connCtxs.size(); i++){
        if (connCtxs[i].getHandle() == connInfo.getConnHandle()){
            removeIdx = i;
            break;
        }
    }
    if(removeIdx>=0){
        connCtxs.erase(connCtxs.begin()+removeIdx);
    }
    xSemaphoreGive(clisMtx);
    NimBLEDevice::startAdvertising();
}

void BLELNServer::start(Preferences *prefs) {
    clisMtx= xSemaphoreCreateMutex();
    keyExTxMtx = xSemaphoreCreateMutex();
    txMtx = xSemaphoreCreateMutex();
    g_rxQueue = xQueueCreate(20, sizeof(RxPacket));

    xTaskCreatePinnedToCore(
            [](void* arg){
                static_cast<BLELNServer*>(arg)->rxWorker();
                vTaskDelete(nullptr);
            },
            "BLELNrx", 4096, this, 5, nullptr, 1);

    BLELNBase::load_or_init_psk(prefs, g_psk_salt, &g_epoch);
    BLELNBase::rng_init();

    NimBLEDevice::init("ESP32C3 Secure ECDH");
    NimBLEDevice::setMTU(247);

    // Security: bonding + MITM + LE Secure Connections
    NimBLEDevice::setSecurityAuth(true, true, true);
    // Passkey – generuj losowy 6-cyfrowy przy starcie (demo)
    uint32_t pass = 123456;
    NimBLEDevice::setSecurityPasskey(pass);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    srv = NimBLEDevice::createServer();
    srv->setCallbacks(this);

    auto* svc = srv->createService(BLELNBase::SERVICE_UUID);

    chKeyExTx = svc->createCharacteristic(BLELNBase::KEYEX_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    chKeyExRx = svc->createCharacteristic(BLELNBase::KEYEX_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
    chDataTx  = svc->createCharacteristic(BLELNBase::DATA_TX_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC);
    chDataRx  = svc->createCharacteristic(BLELNBase::DATA_RX_UUID,  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

    chKeyExTx->setCallbacks(new KeyExTxClb(this));
    chKeyExRx->setCallbacks(new KeyExRxClb(this));
    chDataRx->setCallbacks(new DataRxClb(this));

    svc->start();

    auto* adv = NimBLEDevice::getAdvertising();
    adv->setName("MioGiapicco Light");
    adv->addServiceUUID(BLELNBase::SERVICE_UUID);
    adv->enableScanResponse(true);

    NimBLEDevice::startAdvertising();

    g_lastRotateMs = millis();
}

bool BLELNServer::getConnContext(uint16_t h, BLELNConnCtx** ctx) {
    *ctx = nullptr;

    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(100))!=pdTRUE) return false;
    for (auto &c : connCtxs){
        if (c.getHandle() == h){
            *ctx = &c;
            break;
        }
    }
    xSemaphoreGive(clisMtx);

    return true;
}

void BLELNServer::setChDataTx(const std::string &s) {
    chDataTx->setValue(s.c_str());
}

void BLELNServer::notifyChDataTx() {
    chDataTx->notify();
}

void BLELNServer::maybe_rotate(Preferences *prefs) {
    uint32_t now = millis();
    if (now - g_lastRotateMs >= ROTATE_MS) {
        BLELNBase::rotate_psk(prefs, g_psk_salt, &g_epoch);
        g_lastRotateMs = now;
    }
}

uint32_t BLELNServer::onPassKeyDisplay() {
    return 123456;
}

void BLELNServer::sendKeyToClient(BLELNConnCtx *cx) {
    // KEYEX_TX: [ver=1][epoch:4B][salt:32B][srvPub:65B][srvNonce:12B]
    std::string keyex;
    keyex.push_back(1);
    keyex.append((const char*)&g_epoch, 4); // LE
    keyex.append((const char*)g_psk_salt, 32);
    keyex.append(cx->getEncData()->getPublicKeyString());
    keyex.append(cx->getEncData()->getNonceString());


    if(xSemaphoreTake(keyExTxMtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        chKeyExTx->setValue(keyex);
        chKeyExTx->notify(cx->getHandle());
        cx->setKeySent(true);
        xSemaphoreGive(keyExTxMtx);
    } else {
        Serial.println("Failed locking semaphore! (Key Exchange)");
    }
}

void BLELNServer::appendToQueue(uint16_t h, const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    RxPacket pkt{ h, m.size(), heapBuf };
    if (xQueueSend(g_rxQueue, &pkt, 0) != pdPASS) {
        free(heapBuf);
    }
}

void BLELNServer::rxWorker() {
    for (;;) {
        RxPacket pkt{};
        if (xQueueReceive(g_rxQueue, &pkt, portMAX_DELAY) == pdTRUE) {
            BLELNConnCtx *cx;
            if(getConnContext(pkt.conn, &cx) and (cx!= nullptr)) {
                std::string v(reinterpret_cast<char*>(pkt.buf), pkt.len);

                std::string plain;
                if (!cx->getEncData()->decryptAESGCM((const uint8_t *) v.data(), v.size(), plain)) {
                    Serial.printf("[DATA] decrypt fail (conn=%u)\n\r", cx->getHandle());
                    return;
                }
                // prosty filtr: ogranicz długość i usuń 0x00
                if (plain.size() > 200) plain.resize(200);
                for (auto &ch: plain) if (ch == '\0') ch = ' ';

                std::string dat = "[RX]: (" + std::to_string(cx->getHandle()) + ") - " + plain;
                Serial.println(dat.c_str());

                sendEncrypted(cx, "$HI,Server");

                free(pkt.buf);
            }
        }
    }
}

bool BLELNServer::sendEncrypted(BLELNConnCtx *cx, const std::string &msg) {
    std::string encrypted;
    if(!cx->getEncData()->encryptAESGCM(msg, encrypted)){
        Serial.println("Encrypt failed");
        return false;
    }

    chDataTx->setValue(encrypted);
    chDataTx->notify(cx->getHandle());
    return true;
}

void BLELNServer::onDataWrite(NimBLECharacteristic *c, NimBLEConnInfo &info) {
    BLELNConnCtx *cx;
    if(!getConnContext(info.getConnHandle(), &cx)){
        Serial.println("Failed locking semaphore! (onWrite)");
        return;
    }
    if (!cx) {
        Serial.println("Received message from unknown client");
        return;
    }
    const std::string &v = c->getValue();

    if (v.empty()) {
        return;
    }

    appendToQueue(info.getConnHandle(), v);
}

void BLELNServer::onKeyExRxWrite(NimBLECharacteristic *c, NimBLEConnInfo &info) {
    BLELNConnCtx *cx;
    if(!getConnContext(info.getConnHandle(), &cx)){
        return;
    }
    if (!cx) return;

    const std::string &v = c->getValue();
    // Expect: [ver=1][cliPub:65][cliNonce:12]
    if (v.size()!=1+65+12 || (uint8_t)v[0]!=1) { Serial.println("[HX] bad packet"); return; }

    bool r= cx->getEncData()->deriveSessionKey(&BLELNBase::ctr_drbg, (const uint8_t*)&v[1],
                                               (const uint8_t*)&v[1+65], g_psk_salt,
                                               g_epoch);

    if (!r ) {
        Serial.println("[HX] derive failed"); return;
    }
    cx->setSessionReady(true);
    //chDataTx->setValue("HANDSHAKRE_OK\n");
    //chDataTx->notify();
}

void BLELNServer::onKeyExTxSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) {
    BLELNConnCtx *cx;
    if(!getConnContext(connInfo.getConnHandle(), &cx) or (cx== nullptr)) return;

    if(cx->isSendKeyNeeded()){
        sendKeyToClient(cx);
    }
}
