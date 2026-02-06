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
    workerActionQueue = xQueueCreate(50, sizeof(BLELNWorkerAction));

    // Start worker thread
    runWorker= true;
    xTaskCreatePinnedToCore(
            [](void* arg){
                static_cast<BLELNServer*>(arg)->worker();
                Serial.println("[D] BLELNServer -  rx worker stopped");
                vTaskDelete(nullptr);
            }, "BLELNWorker", 4096, this, 5, &workerTaskHandle, 1);

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
    keyTxClb = new KeyTxClb(this);
    keyRxClb = new KeyRxClb(this);
    dataRxClb = new DataRxClb(this);

    chKeyToCli->setCallbacks(keyTxClb);
    chKeyToSer->setCallbacks(keyRxClb);
    chDataToSer->setCallbacks(dataRxClb);

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
    uint32_t startWait = millis();
    while (workerTaskHandle != nullptr && (millis() - startWait < 5000)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (workerTaskHandle != nullptr) {
        Serial.println("[E] BLELNServer - Worker stuck, killing force");
        vTaskDelete(workerTaskHandle);
        workerTaskHandle = nullptr;
    }

    // Disconnect every client
    if (srv!=nullptr) {
        if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(1000))==pdTRUE) {
            for (auto &c: connCtxs) {
                srv->disconnect(c.getHandle());
            }
            xSemaphoreGive(clisMtx);
        }
    }

    if(workerActionQueue) {
        xQueueReset(workerActionQueue);
        vQueueDelete(workerActionQueue);
        workerActionQueue = nullptr;
    }


    // Remove callbacks
    if (chKeyToCli) chKeyToCli->setCallbacks(nullptr);
    if (chKeyToSer) chKeyToSer->setCallbacks(nullptr);
    if (chDataToSer) chDataToSer->setCallbacks(nullptr);

    if (keyTxClb) { delete keyTxClb; keyTxClb = nullptr; }
    if (keyRxClb) { delete keyRxClb; keyRxClb = nullptr; }
    if (dataRxClb) { delete dataRxClb; dataRxClb = nullptr; }

    // Clear context list
    if(xSemaphoreTake(clisMtx, pdMS_TO_TICKS(1000))==pdTRUE) {
        connCtxs.clear();
        xSemaphoreGive(clisMtx);
    }

    // Clear semaphores
    if(clisMtx){
        vSemaphoreDelete(clisMtx);
        clisMtx = nullptr;
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

    auto it= std::find_if(connCtxs.begin(), connCtxs.end(),[h](const BLELNConnCtx &c){
       return c.getHandle()==h;
    });

    if(it!=connCtxs.end()){
        *ctx= &(*it);
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

    BLELNWorkerAction pkt{h, BLELN_WORKER_ACTION_SEND_MESSAGE, msg.size(), heapBuf };
    if (xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
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


    BLELNWorkerAction pkt{UINT16_MAX, BLELN_WORKER_ACTION_SEND_MESSAGE, msg.size(), heapBuf };
    if (xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
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

        BLELNWorkerAction action{};
        if(xSemaphoreTake(clisMtx, 0)==pdTRUE){
            if(xQueueReceive(workerActionQueue, &action, 0)==pdTRUE){
                if(action.type == BLELN_WORKER_ACTION_REGISTER_CONNECTION){
                    worker_registerClient(action.connH);
                    xSemaphoreGive(clisMtx);
                } else if(action.type == BLELN_WORKER_ACTION_DELETE_CONNECTION){
                    worker_deleteClient(action.connH);
                    xSemaphoreGive(clisMtx);
                } else if(action.type==BLELN_WORKER_ACTION_PROCESS_SUBSCRIPTION){
                    xSemaphoreGive(clisMtx);
                    worker_processSubscription(action.connH);
                } else if(action.type==BLELN_WORKER_ACTION_SEND_MESSAGE){
                    xSemaphoreGive(clisMtx);
                    worker_sendMessage(action.connH, action.d, action.dlen);
                } else if(action.type==BLELN_WORKER_ACTION_PROCESS_KEY_RX){
                    xSemaphoreGive(clisMtx);
                    worker_processKeyRx(action.connH, action.d, action.dlen);
                } else if(action.type==BLELN_WORKER_ACTION_PROCESS_DATA_RX){
                    xSemaphoreGive(clisMtx);
                    worker_processDataRx(action.connH, action.d, action.dlen);
                }

                free(action.d);
            } else {
                xSemaphoreGive(clisMtx);
            }
        }

        // Pause task
        if((uxQueueMessagesWaiting(workerActionQueue) > 0)){
            vTaskDelay(pdMS_TO_TICKS(5));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    worker_cleanup();
    workerTaskHandle= nullptr;
}


/// *************** PRIVATE - Methods ***************

void BLELNServer::worker_registerClient(uint16_t h) {
    BLELNConnCtx *c = nullptr;

    if (!getConnContext(h, &c)) {

        connCtxs.emplace_back(h);
        c = &connCtxs.back();

        if (!c->makeSessionKey()) {
            Serial.println("[E] BLELNServer - ECDH keygen fail");
        }
    }
}

void BLELNServer::worker_deleteClient(uint16_t h) {
    connCtxs.remove_if([h](const BLELNConnCtx& c){
        return c.getHandle()==h;
    });
}

void BLELNServer::worker_processSubscription(uint16_t h) {
    BLELNConnCtx *cx;
    if (getConnContext(h, &cx)) {
        if (cx->getState() == BLELNConnCtx::State::Initialised) {
            sendKeyToClient(cx);
        } else {
            Serial.println("[E] BLELNServer - Invalid context state");
        }
    }
}

void BLELNServer::worker_sendMessage(uint16_t h, uint8_t *data, size_t dataLen) {
    if(h==UINT16_MAX){
        std::string m(reinterpret_cast<char*>(data), dataLen);
        for(auto & connCtx : connCtxs){
            _sendEncrypted(&connCtx,m);
        }
    } else {
        BLELNConnCtx *cx;
        if(getConnContext(h, &cx)){
            std::string m(reinterpret_cast<char*>(data), dataLen);
            _sendEncrypted(cx, m);
        }
    }
}

void BLELNServer::worker_processKeyRx(uint16_t h, uint8_t *data, size_t dataLen) {
// If new key message received
    BLELNConnCtx *cx;
    // Find context for client who sent message
    if (getConnContext(h, &cx)) {
        if (cx->getState() == BLELNConnCtx::State::WaitingForKey) {
            // If I'm waiting for clients session key
            // [ver=1][cliPub:65][cliNonce:12]
            if (dataLen != 1 + 65 + 12 || data[0] != 1) {
                Serial.println("[E] BLELNServer - bad key packet");
            } else {
                // Read clients session key
                bool r = cx->getSessionEnc()->deriveFriendsKey(data + 1,
                                                               data + 1 + 65, g_psk_salt,
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
            if (cx->getSessionEnc()->decryptMessage(data, dataLen, plainKeyMsg)) {
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
            if (cx->getSessionEnc()->decryptMessage(data, dataLen, plainKeyMsg)) {
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
            std::string plainKeyMsg;
            if (cx->getSessionEnc()->decryptMessage(data, dataLen, plainKeyMsg)) {
                StringList parts = splitCsvRespectingQuotes(plainKeyMsg);
                if (parts[0] == BLELN_MSG_TITLE_AUTH_OK and parts.size() == 2) {
                    Serial.printf("[I] BLELNServer - client %d authorised\r\n", cx->getHandle());
                    cx->setState(BLELNConnCtx::State::Authorised);
                }
            }
        }
    }
}

void BLELNServer::worker_processDataRx(uint16_t h, uint8_t *data, size_t dataLen) {
    // If new data message in queue
    BLELNConnCtx *cx;
    // Find context for client who sent data message
    if (getConnContext(h, &cx) and (cx != nullptr)) {
        if (cx->getState() == BLELNConnCtx::State::Authorised) {
            std::string v(reinterpret_cast<char *>(data), dataLen);

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
}


void BLELNServer::worker_cleanup() {
    BLELNWorkerAction pkt{};
    while (xQueueReceive(workerActionQueue, &pkt, 0) == pdPASS) {
        vTaskDelay(pdMS_TO_TICKS(10));
        free(pkt.d);
    }
}

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

    BLELNWorkerAction pkt{h, BLELN_WORKER_ACTION_PROCESS_DATA_RX, m.size(), heapBuf };
    if (xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

void BLELNServer::appendToKeyQueue(uint16_t h, const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    BLELNWorkerAction pkt{h, BLELN_WORKER_ACTION_PROCESS_KEY_RX, m.size(), heapBuf };
    if (xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
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

    BLELNWorkerAction pkt{h, BLELN_WORKER_ACTION_REGISTER_CONNECTION, 0, nullptr};
    xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(100));

    NimBLEDevice::startAdvertising();
}

void BLELNServer::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) {
    uint16_t h= connInfo.getConnHandle();

    BLELNWorkerAction pkt{h, BLELN_WORKER_ACTION_DELETE_CONNECTION, 0, nullptr};
    xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(100));

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
        BLELNWorkerAction pkt{h, BLELN_WORKER_ACTION_PROCESS_SUBSCRIPTION, 0, nullptr};
        xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(100));
    } else {
        Serial.println("[D] BLELNServer - Client unsubscribed for KeyTX");
    }
}


void BLELNServer::setOnMessageReceivedCallback(std::function<void(uint16_t cliH, const std::string& msg)> cb) {
    onMsgReceived= std::move(cb);
}







