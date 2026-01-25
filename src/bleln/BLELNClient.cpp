//
// Created by dkulpa on 7.12.2025.
//

#include "BLELNClient.h"

#include <utility>
#include "BLELNBase.h"
#include "SuperString.h"

void BLELNClient::start(const std::string &name, std::function<void(const std::string&)> onServerResponse) {
    NimBLEDevice::init(name);
    NimBLEDevice::setSecurityAuth(true,true,true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setMTU(247);

    Encryption::randomizer_init();
    authStore.loadCert();

    dataRxQueue = xQueueCreate(10, sizeof(RxPacket));
    keyRxQueue = xQueueCreate(5, sizeof(RxPacket));
    onMsgRx= std::move(onServerResponse);
    runWorker= true;
    connMtx= xSemaphoreCreateMutex();
    if(xSemaphoreTake(connMtx, pdMS_TO_TICKS(1000))==pdTRUE){
        delete connCtx;
        xSemaphoreGive(connMtx);
    } else {
        // TODO: Error handling
    }
    xTaskCreatePinnedToCore(
            [](void* arg){
                static_cast<BLELNClient*>(arg)->rxWorker();
                vTaskDelete(nullptr);
            },
            "BLELNrx", 3072, this, 5, nullptr, 1);
}

void BLELNClient::stop() {
    if (scanning) {
        NimBLEScan* scan = NimBLEDevice::getScan();
        if(scan)
            scan->stop();
        scanning = false;
        onScanResult = nullptr;
    }

    runWorker= false;
    if(dataRxQueue)
        xQueueReset(dataRxQueue);

    if(chKeyToCli)
        chKeyToCli->unsubscribe(true);
    if(chDataToCli)
        chDataToCli ->unsubscribe(true);

    if(client!= nullptr){
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        client= nullptr;
    }

    chKeyToCli = nullptr;
    chKeyToSer = nullptr;
    chDataToCli  = nullptr;
    chDataToSer  = nullptr;
    svc       = nullptr;

    onMsgRx = nullptr;

    NimBLEDevice::deinit(true);
}

void BLELNClient::startServerSearch(uint32_t durationMs, const std::string &serverUUID, const std::function<void(const NimBLEAdvertisedDevice *advertisedDevice)>& onResult) {
    scanning = true;
    onScanResult= onResult;
    searchedUUID= serverUUID;
    auto* scan=NimBLEDevice::getScan();
    scan->setScanCallbacks(this, false);
    scan->setActiveScan(true);
    scan->start(durationMs, false, false);
}

void BLELNClient::beginConnect(const NimBLEAdvertisedDevice *advertisedDevice, const std::function<void(bool, int)> &onConnectResult) {
    if(scanning)
        NimBLEDevice::getScan()->stop();
    scanning = false;

    onConRes= onConnectResult;
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(this, false);
    client->connect(advertisedDevice, true, true, true);
}


void BLELNClient::onDiscovered(const NimBLEAdvertisedDevice *advertisedDevice) {
    // Serial.println(advertisedDevice->toString().c_str());
}

void BLELNClient::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(searchedUUID))) {
        NimBLEDevice::getScan()->stop();
        scanning = false;
        if(onScanResult){
            onScanResult(advertisedDevice);
        }
    }
}

void BLELNClient::onScanEnd(const NimBLEScanResults &scanResults, int reason) {
    scanning = false;
    if(onScanResult){
        onScanResult(nullptr);
    }
}

bool BLELNClient::discover() {
    auto* s = client->getService(BLELNBase::SERVICE_UUID);
    if(!s)
        return false;
    svc=s;
    chKeyToCli = s->getCharacteristic(BLELNBase::KEY_TO_CLI_UUID);
    chKeyToSer = s->getCharacteristic(BLELNBase::KEY_TO_SER_UUID);
    chDataToCli  = s->getCharacteristic(BLELNBase::DATA_TO_CLI_UUID);
    chDataToSer  = s->getCharacteristic(BLELNBase::DATA_TO_SER_UUID);

    if(chKeyToCli && chKeyToSer && chDataToCli && chDataToSer) {
        chKeyToCli->subscribe(true,
                              [this](NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                                    bool isNotify) {
                                 this->onKeyTxNotify(pBLERemoteCharacteristic, pData, length, isNotify);
                             });
        chDataToCli->subscribe(true,
                               [this](NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                                   bool isNotify) {
                                this->onDataTxNotify(pBLERemoteCharacteristic, pData, length, isNotify);
                            });
    }

    return chKeyToCli && chKeyToSer && chDataToCli && chDataToSer;
}


/*** Connection context not protected! */
bool BLELNClient::handshake(uint8_t *v, size_t vlen) {
    if (vlen!=1+4+32+65+12 || (uint8_t)v[0]!=1) {
        Serial.printf("[HX] bad keyex len=%u\n",(unsigned)vlen);
        return false;
    }

    uint32_t s_epoch = 0;
    uint8_t  s_salt[32], s_srvPub[65], s_srvNonce[12];

    memcpy(&s_epoch,  &v[1], 4);
    memcpy(s_salt,    &v[1+4], 32);
    memcpy(s_srvPub,  &v[1+4+32], 65);
    memcpy(s_srvNonce,&v[1+4+32+65], 12);

    connCtx->getSessionEnc()->makeMyKeys();

    // [ver=1][cliPub:65][cliNonce:12]
    std::string tx;
    tx.push_back(1);
    tx.append((const char*)connCtx->getSessionEnc()->getMyPub(),65);
    tx.append((const char*)connCtx->getSessionEnc()->getMyNonce(),12);

    if(!chKeyToSer->writeValue(tx, true)){
        Serial.println("[HX] write fail");
        return false;
    }

    connCtx->getSessionEnc()->deriveFriendsKey(s_srvPub, s_srvNonce, s_salt, s_epoch);

    return true;
}

bool BLELNClient::sendEncrypted(const std::string &msg) {
    std::string pkt;

    if(xSemaphoreTake(connMtx, pdMS_TO_TICKS(1000))==pdTRUE){
        if(connCtx!= nullptr and !connCtx->getSessionEnc()->encryptMessage(msg, pkt)){
            xSemaphoreGive(connMtx);
            return false;
        }
        xSemaphoreGive(connMtx);
    } else {
        return false;
    }

    return chDataToSer->writeValue(pkt, false);
}

bool BLELNClient::isConnected() {
    return (client!= nullptr) && (client->isConnected());
}

bool BLELNClient::hasDiscoveredClient() {
    return svc!= nullptr;
}

void BLELNClient::onPassKeyEntry(NimBLEConnInfo &connInfo) {
    NimBLEDevice::injectPassKey(connInfo, 123456);
}

void BLELNClient::onKeyTxNotify(__attribute__((unused)) NimBLERemoteCharacteristic *ch, uint8_t *pData, size_t length,
                                __attribute__((unused)) bool isNotify) {
    NimBLEAttValue v= ch->getValue();

    if(v.size()==0){
        return;
    }

    appendToQueue(v.data(), v.size(), &keyRxQueue);
}

void BLELNClient::onDataTxNotify(NimBLERemoteCharacteristic *ch, __attribute__((unused)) uint8_t *pData,
                                 __attribute__((unused)) size_t length,
                                 __attribute__((unused)) bool isNotify) {
    NimBLEAttValue v =ch->getValue();

    if (v.size()==0) {
        return;
    }

    appendToQueue(v.data(), v.size(), &dataRxQueue);
}

bool BLELNClient::isScanning() const {
    return scanning;
}

void BLELNClient::appendToQueue(const uint8_t *m, size_t mlen, QueueHandle_t *queue) {
    auto* heapBuf = (uint8_t*)malloc(mlen);
    if (!heapBuf) return;
    memcpy(heapBuf, m, mlen);

    RxPacket pkt{ 0,mlen, heapBuf };
    if (xQueueSend(*queue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

void BLELNClient::rxWorker() {
    while(runWorker){
        if(xSemaphoreTake(connMtx, 0)==pdTRUE){
            if(connCtx!= nullptr and connCtx->getState()==BLELNConnCtx::State::New){
                Serial.println("BLELNClient - new connection detected. Discovering...");
                if(discover()){
                    Serial.println("BLELNClient - discover success");
                    connCtx->setState(BLELNConnCtx::State::WaitingForKey);
                } else {
                    Serial.println("BLELNClient - discover failed");
                }
            }
            xSemaphoreGive(connMtx);
        }

        RxPacket keyPkt{};
        if(xQueueReceive(keyRxQueue, &keyPkt, 0)==pdTRUE){
            if(xSemaphoreTake(connMtx, 0)==pdTRUE){
                if(connCtx!= nullptr){
                    if(connCtx->getState()==BLELNConnCtx::State::WaitingForKey){
                        Serial.println("BLELNClient - received session key");
                        if(handshake(keyPkt.buf, keyPkt.len)){
                            Serial.println("BLELNClient - handshake success");
                            connCtx->setState(BLELNConnCtx::State::WaitingForCert);
                        }
                    }  else if(connCtx->getState()==BLELNConnCtx::State::WaitingForCert){
                        std::string plainKeyMsg;
                        Serial.println("BLELNClient - Received message waiihng for cert");
                        if (connCtx->getSessionEnc()->decryptMessage(keyPkt.buf, keyPkt.len, plainKeyMsg)) {
                            Serial.println("BLELNClient - decrypt successful");
                            StringList parts= splitCsvRespectingQuotes(plainKeyMsg);
                            if(parts[0]==BLELN_MSG_TITLE_CERT and parts.size()==3){
                                Serial.println("BLELNClient - Received clients cert with sign");
                                uint8_t gen;
                                uint8_t fMac[6];
                                uint8_t fPubKey[BLELN_DEV_PUB_KEY_LEN];

                                if(authStore.verifyCert(parts[1], parts[2], &gen, fMac, 6, fPubKey, 64)){
                                    connCtx->setCertData(fMac, fPubKey);
                                    sendCertToServer(connCtx);
                                    connCtx->setState(BLELNConnCtx::State::ChallengeResponseCli);
                                } else {
                                    disconnect(BLE_ERR_AUTH_FAIL);
                                    connCtx->setState(BLELNConnCtx::State::AuthFailed);
                                    Serial.println("BLELNClient - WaitingForCert - invalid cert");
                                }
                            } else {
                                Serial.println("BLELNClient - WaitingForCert - wrong message");
                                disconnect(BLE_ERR_AUTH_FAIL);
                                connCtx->setState(BLELNConnCtx::State::AuthFailed);
                            }
                        } else {
                            disconnect(BLE_ERR_AUTH_FAIL);
                            connCtx->setState(BLELNConnCtx::State::AuthFailed);
                        }
                    } else if(connCtx->getState()==BLELNConnCtx::State::ChallengeResponseCli) {
                        std::string plainKeyMsg;
                        if (connCtx->getSessionEnc()->decryptMessage(keyPkt.buf, keyPkt.len, plainKeyMsg)) {

                        } else {
                            disconnect(BLE_ERR_AUTH_FAIL);
                            connCtx->setState(BLELNConnCtx::State::AuthFailed);
                        }
                        // TODO: Receive servers nonce to sign
                    }
                }
                xSemaphoreGive(connMtx);
            }
        }

        RxPacket pkt{};
        if (xQueueReceive(dataRxQueue, &pkt, 0) == pdTRUE) {
            if(xSemaphoreTake(connMtx, pdMS_TO_TICKS(100))==pdTRUE){
                if(connCtx!= nullptr and connCtx->getSessionEnc()->getSessionId() != 0) {
                    if (pkt.len >= 4 + 12 + 16) {
                        std::string plain;
                        if (connCtx->getSessionEnc()->decryptMessage(pkt.buf, pkt.len, plain)) {
                            if (onMsgRx) {
                                onMsgRx(plain);
                            }
                        }
                    }
                }

                xSemaphoreGive(connMtx);
            }

            free(pkt.buf);
        }

        // Pause task
        if((uxQueueMessagesWaiting(dataRxQueue) > 0) or (uxQueueMessagesWaiting(keyRxQueue) > 0)){
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void BLELNClient::onDisconnect(NimBLEClient *pClient, int reason) {
    if(xSemaphoreTake(connMtx, pdMS_TO_TICKS(1000))==pdTRUE){
        delete connCtx;
        xSemaphoreGive(connMtx);
    } else {
        // TODO: Error handling
    }
    NimBLEClientCallbacks::onDisconnect(pClient, reason);
}

void BLELNClient::onConnect(NimBLEClient *pClient) {
    if(onConRes){
        onConRes(true, 0);
    }
    Serial.println("Client connected");
    if(xSemaphoreTake(connMtx, pdMS_TO_TICKS(1000))==pdTRUE){
        connCtx= new BLELNConnCtx(pClient->getConnHandle());
        xSemaphoreGive(connMtx);
    } else {
        // TODO: Error handling
    }

}

void BLELNClient::onConnectFail(NimBLEClient *pClient, int reason) {
    if(xSemaphoreTake(connMtx, pdMS_TO_TICKS(1000))==pdTRUE){
        delete connCtx;
        xSemaphoreGive(connMtx);
    } else {
        // TODO: Error handling
    }
    if(onConRes)
        onConRes(false, reason);
}


void BLELNClient::disconnect(uint8_t reason) {
    Serial.printf("BLELNClient - diconneting: %d", reason);
    chKeyToCli->unsubscribe();
    chDataToCli->unsubscribe();

    svc= nullptr;
    chKeyToCli = nullptr;
    chKeyToSer = nullptr;
    chDataToCli  = nullptr;
    chDataToSer  = nullptr;

    client->disconnect(reason);
    NimBLEDevice::deleteClient(client);
}


void BLELNClient::sendChallengeNonce(BLELNConnCtx *cx) {
    uint8_t nonce[BLELN_TEST_NONCE_LEN];
    Encryption::random_bytes(nonce, BLELN_TEST_NONCE_LEN);
    cx->setTestNonce(nonce);
    std::string base64Nonce= Encryption::base64Encode(nonce, BLELN_TEST_NONCE_LEN);

    std::string msg= BLELN_MSG_TITLE_CHALLENGE_RESPONSE_NONCE;
    msg.append(",").append(base64Nonce);

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToSer->writeValue(encMsg, true);
    } else {
        Serial.println("BLELNClient - failed encrypting cert msg");
    }
}

void BLELNClient::sendChallengeNonceSign(BLELNConnCtx *cx, uint8_t *sign) {
    std::string msg= BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW;
    std::string base64Sign= Encryption::base64Encode(sign, BLELN_NONCE_SIGN_LEN);
    msg.append(",").append(base64Sign);

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToSer->writeValue(encMsg, true);
    } else {
        Serial.println("BLELNClient - failed encrypting cert msg");
    }
}

void BLELNClient::sendCertToServer(BLELNConnCtx *cx) {
    std::string msg=BLELN_MSG_TITLE_CERT;
    msg.append(",").append(authStore.getSignedCert());

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToSer->writeValue(encMsg, true);
    } else {
        Serial.println("BLELNClient - failed encrypting cert msg");
    }
}


