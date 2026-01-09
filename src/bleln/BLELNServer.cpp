//
// Created by dkulpa on 17.08.2025.
//

#include "BLELNServer.h"
#include <utility>
#include "Encryption.h"
#include "SuperString.h"


void BLELNServer::start(Preferences *prefs, const std::string &name, const std::string &uuid) {
    serviceUUID= uuid;

    // Initialize mutexes
    clisMtx= xSemaphoreCreateMutex();
    keyExTxMtx = xSemaphoreCreateMutex();
    txMtx = xSemaphoreCreateMutex();

    // Initialize queues
    dataRxQueue = xQueueCreate(20, sizeof(DataRxPacket));
    keyRxQueue = xQueueCreate(20, sizeof(KeyRxPacket));

    // Start worker thread
    runWorker= true;
    xTaskCreatePinnedToCore(
            [](void* arg){
                static_cast<BLELNServer*>(arg)->worker();
                Serial.println("BLELN Server: rx worker stopped");
                vTaskDelete(nullptr);
            }, "BLELNWorker", 2560, this, 5, nullptr, 1);

    // Initialize encryption salt (or find in memory)
    size_t have = prefs->getBytesLength("salt");
    if (have != 32) {
        Encryption::random_bytes(g_psk_salt, 32);
        g_epoch = 1;
        prefs->putBytes("salt", g_psk_salt, 32);
        prefs->putULong("epoch", g_epoch);
    } else {
        prefs->getBytes("salt", g_psk_salt, 32);
        g_epoch = prefs->getULong("epoch", 1);
        if (g_epoch == 0) g_epoch = 1;
    }
    Encryption::randomizer_init();

    // Init NimBLE
    NimBLEDevice::init(name);
    NimBLEDevice::setMTU(247);

    // Disable BLE securities
    NimBLEDevice::setSecurityAuth(false, false, false);

    // Start BLE server and set callbacks
    srv = NimBLEDevice::createServer();
    srv->setCallbacks(this);

    // Create BLELN service and characteristics
    auto* svc = srv->createService(serviceUUID);
    chKeyExTx = svc->createCharacteristic(BLELNBase::KEYEX_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    chKeyExRx = svc->createCharacteristic(BLELNBase::KEYEX_RX_UUID, NIMBLE_PROPERTY::WRITE);// | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
    chDataTx  = svc->createCharacteristic(BLELNBase::DATA_TX_UUID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);// | NIMBLE_PROPERTY::READ_ENC);
    chDataRx  = svc->createCharacteristic(BLELNBase::DATA_RX_UUID,  NIMBLE_PROPERTY::WRITE);// | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

    // Set characteristics callbacks
    chKeyExTx->setCallbacks(new KeyExTxClb(this));
    chKeyExRx->setCallbacks(new KeyExRxClb(this));
    chDataRx->setCallbacks(new DataRxClb(this));

    // Start BLELN service
    svc->start();

    // Publish/Advertise BLE server
    auto* adv = NimBLEDevice::getAdvertising();
    adv->setName(name);
    adv->addServiceUUID(serviceUUID);
    adv->enableScanResponse(true);

    NimBLEDevice::startAdvertising();
}


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

    if (!c->makeSessionKey()) {
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

void BLELNServer::sendKeyToClient(BLELNConnCtx *cx) {
    // KEYEX_TX: [ver=1][epoch:4B][salt:32B][srvPub:65B][srvNonce:12B]
    std::string keyex;
    keyex.push_back(1);
    keyex.append((const char*)&g_epoch, 4); // LE
    keyex.append((const char*)g_psk_salt, 32);
    keyex.append((const char*)cx->getSessionEnc()->getMyPub(),65);
    keyex.append((const char*)cx->getSessionEnc()->getMyNonce(),12);

    if(xSemaphoreTake(keyExTxMtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        chKeyExTx->setValue(keyex);
        chKeyExTx->notify(cx->getHandle());
        cx->setState(BLELNConnCtx::State::WaitingForKey);
        xSemaphoreGive(keyExTxMtx);
    } else {
        Serial.println("Failed locking semaphore! (Key Exchange)");
    }
}



bool BLELNServer::sendEncrypted(BLELNConnCtx *cx, const std::string &msg) {
    std::string encrypted;
    if(!cx->getSessionEnc()->encryptMessage(msg, encrypted)){
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

    appendToDataQueue(info.getConnHandle(), v);
}

void BLELNServer::onKeyExRxWrite(NimBLECharacteristic *c, NimBLEConnInfo &info) {
    const std::string &v= c->getValue();

    appendToKeyQueue(info.getConnHandle(), v);
}

void BLELNServer::onKeyExTxSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) {
    if(subValue>0) {
        Serial.println("Client subscribed for KeyTX");
        BLELNConnCtx *cx;
        if (!getConnContext(connInfo.getConnHandle(), &cx) or (cx == nullptr)) return;

        if (cx->getState()==BLELNConnCtx::State::Initialised) {
            sendKeyToClient(cx);
        }
    } else {
        Serial.println("Client unsubscribed for KeyTX");
    }
}

bool BLELNServer::sendEncrypted(int i, const std::string &msg) {
    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(100))!=pdTRUE) return false;

    if(i<connCtxs.size()){
        sendEncrypted(&connCtxs[i], msg);
    }
    xSemaphoreGive(clisMtx);

    return true;
}

bool BLELNServer::sendEncrypted(const std::string &msg) {
    for(int i=0; i<connCtxs.size(); i++){
        if(!sendEncrypted(i, msg)) return false;
    }

    return true;
}

void BLELNServer::stop() {
    NimBLEDevice::stopAdvertising();
    // Stop rx worker
    runWorker= false;

    if(dataRxQueue)
        xQueueReset(dataRxQueue);

    // Disconnect every client
    if (srv!=nullptr) {
        if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(300))==pdTRUE) {
            for (auto &c: connCtxs) {
                srv->disconnect(c.getHandle());
            }
            xSemaphoreGive(clisMtx);
        }
    }

    // Remove callbacks
    if (chKeyExTx)
        chKeyExTx->setCallbacks(nullptr);
    if (chKeyExRx)
        chKeyExRx->setCallbacks(nullptr);
    if (chDataRx)
        chDataRx ->setCallbacks(nullptr);

    // Clear semaphores
    if(clisMtx){
        vSemaphoreDelete(clisMtx);
        clisMtx = nullptr;
    }
    if(keyExTxMtx){
        vSemaphoreDelete(keyExTxMtx);
        keyExTxMtx = nullptr;
    }
    if(txMtx){
        vSemaphoreDelete(txMtx);
        txMtx = nullptr;
    }

    // Clear context list
    connCtxs.clear();

    // Reset pointer and callback
    chKeyExTx = nullptr;
    chKeyExRx = nullptr;
    chDataTx  = nullptr;
    chDataRx  = nullptr;
    srv       = nullptr;
    onMsgReceived = nullptr;

    NimBLEDevice::deinit(true);
}

void BLELNServer::setOnMessageReceivedCallback(std::function<void(uint16_t cliH, const std::string& msg)> cb) {
    onMsgReceived= std::move(cb);
}

bool BLELNServer::sendEncrypted(uint16_t h, const std::string &msg) {
    BLELNConnCtx *connCtx= nullptr;
    getConnContext(h, &connCtx);

    if(connCtx!= nullptr) {
        if (xSemaphoreTake(clisMtx, pdMS_TO_TICKS(100)) != pdTRUE) return false;
        sendEncrypted(connCtx, msg);
        xSemaphoreGive(clisMtx);
        return true;
    }

    return false;
}

void BLELNServer::startOtherServerSearch(uint32_t durationMs, const std::string &otherUUID, const std::function<void(bool)>& onResult) {
    scanning = true;
    onScanResult= onResult;
    searchedUUID= otherUUID;
    auto* scan=NimBLEDevice::getScan();
    scan->setScanCallbacks(this, false);
    scan->setActiveScan(true);
    scan->start(durationMs, false, false);
}

void BLELNServer::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(searchedUUID))) {
        scanning = false;
        if(onScanResult){
            onScanResult(true);
        }
    }
}

void BLELNServer::onScanEnd(const NimBLEScanResults &scanResults, int reason) {
    scanning = false;
    if(onScanResult){
        onScanResult(false);
    }
}

bool BLELNServer::noClientsConnected() {
    uint8_t clisCnt=1;

    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(50))==pdTRUE){
        clisCnt= connCtxs.size();
        xSemaphoreGive(clisMtx);
    }

    return clisCnt==0;
}

void BLELNServer::worker() {
    while(runWorker){
        // Check key queue
        KeyRxPacket keyPkt{};
        if(xQueueReceive(keyRxQueue, &keyPkt, 0)==pdTRUE){
            // If new key message received
            BLELNConnCtx *cx;
            // Find context for client who sent message
            if(getConnContext(keyPkt.conn, &cx) and (cx!= nullptr)) {
                if(cx->getState()==BLELNConnCtx::State::WaitingForKey){
                    // If I'm waiting for clients session key
                    // [ver=1][cliPub:65][cliNonce:12]
                    if (keyPkt.len!=1+65+12 || keyPkt.buf[0]!=1) {
                        Serial.println("[HX] bad packet");
                    } else {
                        // Read clients session key
                        bool r= cx->getSessionEnc()->deriveFriendsKey(keyPkt.buf+1,
                                                                      keyPkt.buf+1+65, g_psk_salt,
                                                                      g_epoch);
                        if (r) {
                            cx->setState(BLELNConnCtx::State::WaitingForCert);
                            std::string msg="$CERT,";
                            msg.append(authStore.getSignedCert());
                            sendEncrypted(cx, msg);
                        } else {
                            Serial.println("[HX] derive failed");
                        }
                    }
                } else if(cx->getState()==BLELNConnCtx::State::WaitingForCert){
                    // If I'm waiting for clients certificate
                    std::string plainKeyMsg;
                    if (cx->getSessionEnc()->decryptMessage(keyPkt.buf, keyPkt.len, plainKeyMsg)) {
                        StringList parts= splitCsvRespectingQuotes(plainKeyMsg);
                        if(parts[0]=="$CERT" and parts.size()==3){
                            if(authStore.verifyCert(parts[1], parts[2])){
                                // TODO: Derive client public key and ID
                                // TODO: Random and send nonce
                                cx->setState(BLELNConnCtx::State::ChallengeResponseCli);
                            } else {
                                // TODO: Auth failed
                                cx->setState(BLELNConnCtx::State::AuthFailed);
                            }
                        } else {
                            // TODO: Failed auth
                            cx->setState(BLELNConnCtx::State::AuthFailed);
                        }
                    } else {
                        // TODO: Failed auth
                        cx->setState(BLELNConnCtx::State::AuthFailed);
                    }
                } else if(cx->getState()==BLELNConnCtx::State::ChallengeResponseCli){
                    // If I'm waiting for clients challenge response
                    // TODO: Verify response
                    cx->setState(BLELNConnCtx::State::Authorised);
                    // TODO: If wrong response -> disconnect
                }
            }

            free(keyPkt.buf);
        }

        // Check data queue
        DataRxPacket dataPkt{};
        if (xQueueReceive(dataRxQueue, &dataPkt, 0) == pdTRUE) {
            // If new data message in queue
            BLELNConnCtx *cx;
            // Find context for client who sent data message
            if(getConnContext(dataPkt.conn, &cx) and (cx!= nullptr)) {
                if(cx->getState()==BLELNConnCtx::State::Authorised) {
                    std::string v(reinterpret_cast<char *>(dataPkt.buf), dataPkt.len);

                    std::string plain;
                    if (cx->getSessionEnc()->decryptMessage((const uint8_t *) v.data(), v.size(), plain)) {
                        if (plain.size() > 200) plain.resize(200);
                        for (auto &ch: plain) if (ch == '\0') ch = ' ';

                        if (onMsgReceived)
                            onMsgReceived(cx->getHandle(), plain);
                    } else {
                        // TODO: Inform error
                    }
                } else {
                    // Unauthorised client send message!
                    // TODO: Disconnect this client
                }
            }

            free(dataPkt.buf);
        }

        // Pause task
        if((uxQueueMessagesWaiting(dataRxQueue) > 0) or (uxQueueMessagesWaiting(keyRxQueue) > 0)){
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    // TODO: Cleanup queues
}

void BLELNServer::appendToDataQueue(uint16_t h, const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    DataRxPacket pkt{h, m.size(), heapBuf };
    if (xQueueSend(dataRxQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

void BLELNServer::appendToKeyQueue(uint16_t h, const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    KeyRxPacket pkt{h, m.size(), heapBuf };
    if (xQueueSend(keyRxQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

