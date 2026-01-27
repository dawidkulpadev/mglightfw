//
// Created by dkulpa on 17.08.2025.
//

#include "BLELNServer.h"
#include <utility>
#include "Encryption.h"
#include "SuperString.h"

/// *************** PUBLIC ***************

void BLELNServer::start(Preferences *prefs, const std::string &name, const std::string &uuid) {
    serviceUUID= uuid;

    // Initialize mutexes
    clisMtx= xSemaphoreCreateMutex();

    // Initialize queues
    dataRxQueue = xQueueCreate(20, sizeof(BLELNMsgQueuePacket));
    keyRxQueue = xQueueCreate(20, sizeof(BLELNMsgQueuePacket));
    newClientsQueue = xQueueCreate(10, sizeof(uint16_t));
    subscriptionQueue = xQueueCreate(10, sizeof(uint16_t));
    disconnectedClientsQueue= xQueueCreate(10, sizeof(uint16_t));
    msgsToSendQueue= xQueueCreate(10, sizeof(BLELNMsgQueuePacket));

    // Start worker thread
    runWorker= true;
    xTaskCreatePinnedToCore(
            [](void* arg){
                static_cast<BLELNServer*>(arg)->worker();
                Serial.println("[D] BLELNServer -  rx worker stopped");
                vTaskDelete(nullptr);
            }, "BLELNWorker", 4096, this, 5, nullptr, 1);

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
    authStore.loadCert();

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
    chKeyToCli = svc->createCharacteristic(BLELNBase::KEY_TO_CLI_UUID, NIMBLE_PROPERTY::NOTIFY);
    chKeyToSer = svc->createCharacteristic(BLELNBase::KEY_TO_SER_UUID, NIMBLE_PROPERTY::WRITE);// | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
    chDataToCli  = svc->createCharacteristic(BLELNBase::DATA_TO_CLI_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);// | NIMBLE_PROPERTY::READ_ENC);
    chDataToSer  = svc->createCharacteristic(BLELNBase::DATA_TO_SER_UUID, NIMBLE_PROPERTY::WRITE);// | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

    // Set characteristics callbacks
    chKeyToCli->setCallbacks(new KeyTxClb(this));
    chKeyToSer->setCallbacks(new KeyRxClb(this));
    chDataToSer->setCallbacks(new DataRxClb(this));

    // Start BLELN service
    svc->start();

    // Publish/Advertise BLE server
    auto* adv = NimBLEDevice::getAdvertising();
    adv->setName(name);
    adv->addServiceUUID(serviceUUID);
    adv->enableScanResponse(true);

    NimBLEDevice::startAdvertising();
}

void BLELNServer::stop() {
    NimBLEDevice::stopAdvertising();
    // Stop rx worker
    runWorker= false;

    if(dataRxQueue)
        xQueueReset(dataRxQueue);

    // Disconnect every client
    if (srv!=nullptr) {
        if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(1000))==pdTRUE) {
            for (auto &c: connCtxs) {
                srv->disconnect(c.getHandle());
            }
            xSemaphoreGive(clisMtx);
        }
    }

    // Remove callbacks
    if (chKeyToCli)
        chKeyToCli->setCallbacks(nullptr);
    if (chKeyToSer)
        chKeyToSer->setCallbacks(nullptr);
    if (chDataToSer)
        chDataToSer ->setCallbacks(nullptr);

    // Clear semaphores
    if(clisMtx){
        vSemaphoreDelete(clisMtx);
        clisMtx = nullptr;
    }

    // Clear context list
    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(1000))==pdTRUE) {
        connCtxs.clear();
        xSemaphoreGive(clisMtx);
    }

    // Reset pointer and callback
    chKeyToCli = nullptr;
    chKeyToSer = nullptr;
    chDataToCli  = nullptr;
    chDataToSer  = nullptr;
    srv       = nullptr;
    onMsgReceived = nullptr;

    NimBLEDevice::deinit(true);
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

/*** Not multithreading safe */
bool BLELNServer::getConnContext(uint16_t h, BLELNConnCtx** ctx) {
    *ctx = nullptr;

    for (auto &c : connCtxs){
        if (c.getHandle() == h){
            *ctx = &c;
            break;
        }
    }

    return *ctx!= nullptr;
}

/*** Multithreading safe */
bool BLELNServer::noClientsConnected() {
    uint8_t clisCnt=1;

    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(50))==pdTRUE){
        clisCnt= connCtxs.size();
        xSemaphoreGive(clisMtx);
    }

    return clisCnt==0;
}

/*** Not multithreading safe */
bool BLELNServer::_sendEncrypted(BLELNConnCtx *cx, const std::string &msg) {
    std::string encrypted;
    if(!cx->getSessionEnc()->encryptMessage(msg, encrypted)){
        Serial.println("[E] BLELNServer - Encrypt failed");
        return false;
    }

    chDataToCli->setValue(encrypted);
    chDataToCli->notify(cx->getHandle());
    return true;
}

/*** Multithreading safe */
bool BLELNServer::sendEncrypted(uint16_t h, const std::string &msg) {
    auto* heapBuf = (uint8_t*)malloc(msg.size());
    if (!heapBuf) return false;
    memcpy(heapBuf, msg.data(), msg.size());

    BLELNMsgQueuePacket pkt{h, msg.size(), heapBuf };
    if (xQueueSend(msgsToSendQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
        return false;
    }

    return true;
}

/*** Multithreading safe */
bool BLELNServer::sendEncryptedToAll(const std::string &msg) {
    auto* heapBuf = (uint8_t*)malloc(msg.size());
    if (!heapBuf) return false;
    memcpy(heapBuf, msg.data(), msg.size());


    BLELNMsgQueuePacket pkt{UINT16_MAX, msg.size(), heapBuf };
    if (xQueueSend(msgsToSendQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
        return false;
    }

    return true;
}

void BLELNServer::worker() {
    while(runWorker){
        if((millis() - lastWaterMarkPrint) >= 10000) {
            UBaseType_t freeWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("[D] BLELNServer - stack free: %u\n\r",
                          freeWords);
            lastWaterMarkPrint= millis();
        }


        // Check new clients queue
        uint16_t newConnHandle;
        if(xSemaphoreTake(clisMtx, 0)==pdTRUE) {
            if (xQueueReceive(newClientsQueue, &newConnHandle, 0) == pdTRUE) {
                BLELNConnCtx *c = nullptr;

                if (!getConnContext(newConnHandle, &c)) {

                    connCtxs.emplace_back(newConnHandle);
                    c = (connCtxs.end() - 1).base();

                    if (!c->makeSessionKey()) {
                        Serial.println("[E] BLELNServer - ECDH keygen fail");
                    }
                }
            }

            // Check disconnected clients queue
            uint16_t disconnectedConnHandle;
            if (xQueueReceive(disconnectedClientsQueue, &disconnectedConnHandle, 0) == pdTRUE) {
                int removeIdx = -1;

                for (int i = 0; i < connCtxs.size(); i++) {
                    if (connCtxs[i].getHandle() == disconnectedConnHandle) {
                        removeIdx = i;
                        break;
                    }
                }
                if (removeIdx >= 0) {
                    connCtxs.erase(connCtxs.begin() + removeIdx);
                }
            }

            xSemaphoreGive(clisMtx);
        }

        // Check subscription queue
        uint16_t subscribeConnHandle;
        if(xQueueReceive(subscriptionQueue, &subscribeConnHandle, 0)==pdTRUE){
            BLELNConnCtx *cx;
            if (getConnContext(subscribeConnHandle, &cx)) {
                if (cx->getState() == BLELNConnCtx::State::Initialised) {
                    sendKeyToClient(cx);
                } else {
                    Serial.println("[E] BLELNServer - Invalid context state");
                }
            }
        }

        // Check user msgs to send queue
        BLELNMsgQueuePacket sendPkt{};
        if(xQueueReceive(msgsToSendQueue, &sendPkt, 0)==pdTRUE){
            if(sendPkt.conn==UINT16_MAX){
                std::string m(reinterpret_cast<char*>(sendPkt.buf), sendPkt.len);
                for(auto & connCtx : connCtxs){
                    _sendEncrypted(&connCtx,m);
                }
            } else {
                BLELNConnCtx *cx;
                if(getConnContext(sendPkt.conn, &cx)){
                    std::string m(reinterpret_cast<char*>(sendPkt.buf), sendPkt.len);
                    _sendEncrypted(cx, m);
                }
            }

            free(sendPkt.buf);
        }

        // Check key queue
        BLELNMsgQueuePacket keyPkt{};
        if (xQueueReceive(keyRxQueue, &keyPkt, 0) == pdTRUE) {
            // If new key message received
            BLELNConnCtx *cx;
            // Find context for client who sent message
            if (getConnContext(keyPkt.conn, &cx)) {
                if (cx->getState() == BLELNConnCtx::State::WaitingForKey) {
                    // If I'm waiting for clients session key
                    // [ver=1][cliPub:65][cliNonce:12]
                    if (keyPkt.len != 1 + 65 + 12 || keyPkt.buf[0] != 1) {
                        Serial.println("[E] BLELNServer - bad key packet");
                    } else {
                        // Read clients session key
                        bool r = cx->getSessionEnc()->deriveFriendsKey(keyPkt.buf + 1,
                                                                       keyPkt.buf + 1 + 65, g_psk_salt,
                                                                       g_epoch);
                        if (r) {
                            cx->setState(BLELNConnCtx::State::WaitingForCert);
                            sendCertToClient(cx);
                        } else {
                            Serial.println("[E] BLELNServer - derive failed");
                        }
                    }
                } else if (cx->getState() == BLELNConnCtx::State::WaitingForCert) {
                    // If I'm waiting for clients certificate
                    std::string plainKeyMsg;
                    if (cx->getSessionEnc()->decryptMessage(keyPkt.buf, keyPkt.len, plainKeyMsg)) {
                        StringList parts = splitCsvRespectingQuotes(plainKeyMsg);
                        if (parts[0] == BLELN_MSG_TITLE_CERT and parts.size() == 3) {
                            uint8_t gen;
                            uint8_t fMac[6];
                            uint8_t fPubKey[BLELN_DEV_PUB_KEY_LEN];

                            if (authStore.verifyCert(parts[1], parts[2], &gen, fMac, 6, fPubKey, 64)) {
                                cx->setCertData(fMac, fPubKey);
                                sendChallengeNonce(cx);
                                cx->setState(BLELNConnCtx::State::ChallengeResponseCli);
                            } else {
                                disconnectClient(cx, BLE_ERR_AUTH_FAIL);
                                cx->setState(BLELNConnCtx::State::AuthFailed);
                                Serial.println("[E] BLELNServer - WaitingForCert - invalid cert");
                            }
                        } else {
                            Serial.println("[E] BLELNServer - WaitingForCert - wrong message");
                            disconnectClient(cx, BLE_ERR_AUTH_FAIL);
                            cx->setState(BLELNConnCtx::State::AuthFailed);
                        }
                    } else {
                        disconnectClient(cx, BLE_ERR_AUTH_FAIL);
                        cx->setState(BLELNConnCtx::State::AuthFailed);
                    }
                } else if (cx->getState() == BLELNConnCtx::State::ChallengeResponseCli) {
                    // If I'm waiting for clients challenge response
                    std::string plainKeyMsg;
                    if (cx->getSessionEnc()->decryptMessage(keyPkt.buf, keyPkt.len, plainKeyMsg)) {
                        StringList parts = splitCsvRespectingQuotes(plainKeyMsg);
                        if (parts[0] == BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW_AND_NONCE and parts.size() == 3) {
                            uint8_t nonceSign[BLELN_NONCE_SIGN_LEN];
                            Encryption::base64Decode(parts[1], nonceSign, BLELN_NONCE_SIGN_LEN);
                            if (cx->verifyChallengeResponseAnswer(nonceSign)) {
                                uint8_t nonce[BLELN_TEST_NONCE_LEN];            // Clients nonce
                                uint8_t friendsNonceSign[BLELN_NONCE_SIGN_LEN]; // Clients nonce I have signed
                                Encryption::base64Decode(parts[2], nonce, BLELN_TEST_NONCE_LEN);
                                authStore.signData(nonce, BLELN_TEST_NONCE_LEN, friendsNonceSign);
                                sendChallengeNonceSign(cx, friendsNonceSign);
                                cx->setState(BLELNConnCtx::State::ChallengeResponseSer);
                            } else {
                                Serial.println("[E] BLELNServer - ChallengeResponseCli - invalid sign");
                                disconnectClient(cx, BLE_ERR_AUTH_FAIL);
                                cx->setState(BLELNConnCtx::State::AuthFailed);
                            }
                        } else {
                            Serial.println("[E] BLELNServer - ChallengeResponseCli - wrong message");
                            disconnectClient(cx, BLE_ERR_AUTH_FAIL);
                            cx->setState(BLELNConnCtx::State::AuthFailed);
                        }
                    }
                } else if (cx->getState() == BLELNConnCtx::State::ChallengeResponseSer) {
                    // If I'm waiting for clients challenge response
                    std::string plainKeyMsg;
                    if (cx->getSessionEnc()->decryptMessage(keyPkt.buf, keyPkt.len, plainKeyMsg)) {
                        StringList parts = splitCsvRespectingQuotes(plainKeyMsg);
                        if (parts[0] == BLELN_MSG_TITLE_AUTH_OK and parts.size() == 2) {
                            Serial.printf("[I] BLELNServer - client %d authorised\r\n", cx->getHandle());
                            cx->setState(BLELNConnCtx::State::Authorised);
                        }
                    }
                }
            }

            free(keyPkt.buf);
        }

        // Check data queue
        BLELNMsgQueuePacket dataPkt{};
        if (xQueueReceive(dataRxQueue, &dataPkt, 0) == pdTRUE) {
            // If new data message in queue
            BLELNConnCtx *cx;
            // Find context for client who sent data message
            if (getConnContext(dataPkt.conn, &cx) and (cx != nullptr)) {
                if (cx->getState() == BLELNConnCtx::State::Authorised) {
                    std::string v(reinterpret_cast<char *>(dataPkt.buf), dataPkt.len);

                    std::string plain;
                    if (cx->getSessionEnc()->decryptMessage((const uint8_t *) v.data(), v.size(), plain)) {
                        if (plain.size() > 200) plain.resize(200);
                        for (auto &ch: plain) if (ch == '\0') ch = ' ';

                        if (onMsgReceived)
                            onMsgReceived(cx->getHandle(), plain);
                    } else {
                        Serial.println("[E] BLELNServer - failed to decrypt data message");
                    }
                } else {
                    disconnectClient(cx, BLE_ERR_CONN_REJ_SECURITY);
                }
            }

            free(dataPkt.buf);
        }

        // Pause task
        if((uxQueueMessagesWaiting(dataRxQueue) > 0) or (uxQueueMessagesWaiting(keyRxQueue) > 0)){
            vTaskDelay(pdMS_TO_TICKS(5));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    worker_cleanup();
}

void BLELNServer::worker_cleanup() {
    BLELNMsgQueuePacket pkt{};
    while (xQueueReceive(dataRxQueue, &pkt, 0) == pdPASS) {
        vTaskDelay(pdMS_TO_TICKS(10));
        free(pkt.buf);
    }

    while (xQueueReceive(keyRxQueue, &pkt, 0) == pdPASS) {
        vTaskDelay(pdMS_TO_TICKS(10));
        free(pkt.buf);
    }
}


/// *************** PRIVATE - Methods ***************

void BLELNServer::sendKeyToClient(BLELNConnCtx *cx) {
    // KEYEX_TX: [ver=1][epoch:4B][salt:32B][srvPub:65B][srvNonce:12B]
    std::string keyex;
    keyex.push_back(1);
    keyex.append((const char*)&g_epoch, 4); // LE
    keyex.append((const char*)g_psk_salt, 32);
    keyex.append((const char*)cx->getSessionEnc()->getMyPub(),65);
    keyex.append((const char*)cx->getSessionEnc()->getMyNonce(),12);

    Serial.println("Sending key to client");

    chKeyToCli->setValue(keyex);
    chKeyToCli->notify(cx->getHandle());
    cx->setState(BLELNConnCtx::State::WaitingForKey);

}

void BLELNServer::sendCertToClient(BLELNConnCtx *cx) {
    std::string msg=BLELN_MSG_TITLE_CERT;
    msg.append(",").append(authStore.getSignedCert());

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToCli->setValue(encMsg);
        chKeyToCli->notify(cx->getHandle());
    } else {
        Serial.println("[E] BLELNServer - BLELNServer - failed encrypting cert msg");
    }
}

void BLELNServer::sendChallengeNonce(BLELNConnCtx *cx) {
    cx->generateTestNonce();
    std::string base64Nonce= cx->getTestNonceBase64();

    std::string msg= BLELN_MSG_TITLE_CHALLENGE_RESPONSE_NONCE;
    msg.append(",").append(base64Nonce);

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToCli->setValue(encMsg);
        chKeyToCli->notify(cx->getHandle());
    } else {
        Serial.println("[E] BLELNServer - failed encrypting cert msg");
    }
}

void BLELNServer::sendChallengeNonceSign(BLELNConnCtx *cx, uint8_t *sign) {
    std::string msg= BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW;
    std::string base64Sign= Encryption::base64Encode(sign, BLELN_NONCE_SIGN_LEN);
    msg.append(",").append(base64Sign);

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToCli->setValue(encMsg);
        chKeyToCli->notify(cx->getHandle());
    } else {
        Serial.println("[E] BLELNServer - failed encrypting cert msg");
    }
}

void BLELNServer::disconnectClient(BLELNConnCtx *cx, uint8_t reason){
    if (srv!=nullptr) {
        srv->disconnect(cx->getHandle(), reason);
    }
}

void BLELNServer::appendToDataQueue(uint16_t h, const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    BLELNMsgQueuePacket pkt{h, m.size(), heapBuf };
    if (xQueueSend(dataRxQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

void BLELNServer::appendToKeyQueue(uint16_t h, const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    BLELNMsgQueuePacket pkt{h, m.size(), heapBuf };
    if (xQueueSend(keyRxQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

/// *************** PRIVATE - Callbacks ***************

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

void BLELNServer::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
    uint16_t h= connInfo.getConnHandle();
    xQueueSend(newClientsQueue, &h, pdMS_TO_TICKS(100));

    NimBLEDevice::startAdvertising();
}

void BLELNServer::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) {
    uint16_t h= connInfo.getConnHandle();
    xQueueSend(disconnectedClientsQueue, &h, pdMS_TO_TICKS(100));

    NimBLEDevice::startAdvertising();
}

void BLELNServer::onDataWrite(NimBLECharacteristic *c, NimBLEConnInfo &info) {
    const std::string &v = c->getValue();

    if (!v.empty()) {
        appendToDataQueue(info.getConnHandle(), v);
    }
}


void BLELNServer::onKeyToSerWrite(NimBLECharacteristic *c, NimBLEConnInfo &info) {
    const std::string &v = c->getValue();

    if (!v.empty()) {
        appendToKeyQueue(info.getConnHandle(), v);
    }
}


void BLELNServer::onKeyToCliSubscribe(__attribute__((unused)) NimBLECharacteristic *pCharacteristic,
                                      NimBLEConnInfo &connInfo, uint16_t subValue) {

    if(subValue>0) {
        uint16_t h= connInfo.getConnHandle();
        xQueueSend(subscriptionQueue, &h, pdMS_TO_TICKS(100));
    } else {
        Serial.println("[D] BLELNServer - Client unsubscribed for KeyTX");
    }
}


void BLELNServer::setOnMessageReceivedCallback(std::function<void(uint16_t cliH, const std::string& msg)> cb) {
    onMsgReceived= std::move(cb);
}


