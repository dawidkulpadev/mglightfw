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

    delete connCtx;
    connCtx= nullptr;

    workerActionQueue= xQueueCreate(30, sizeof(BLELNWorkerAction));
    onMsgRx= std::move(onServerResponse);
    runWorker= true;

    xTaskCreatePinnedToCore(
            [](void* arg){
                static_cast<BLELNClient *>(arg)->worker();
                vTaskDelete(nullptr);
            },
            "BLELNrx", 4096, this, 5, &workerTaskHandle, 1);
}

void BLELNClient::stop() {
    if (scanning) {
        NimBLEScan* scan = NimBLEDevice::getScan();
        if(scan) scan->stop();
        scanning = false;
        onScanResult = nullptr;
    }

    runWorker = false;
    if (workerTaskHandle != nullptr) {
        while (workerTaskHandle != nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if(client!= nullptr){
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        client= nullptr;
    }

    if(workerActionQueue) {
        BLELNWorkerAction pkt{};
        while (xQueueReceive(workerActionQueue, &pkt, 0) == pdPASS) {
            free(pkt.d);
        }
        vQueueDelete(workerActionQueue); // Usuń kolejkę
        workerActionQueue = nullptr;
    }

    if(chKeyToCli) chKeyToCli->unsubscribe();
    if(chDataToCli) chDataToCli->unsubscribe();

    chKeyToCli   = nullptr;
    chKeyToSer   = nullptr;
    chDataToCli  = nullptr;
    chDataToSer  = nullptr;
    svc          = nullptr;

    if(connCtx) {
        delete connCtx;
        connCtx = nullptr;
    }

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
        return false;
    }

    connCtx->getSessionEnc()->deriveFriendsKey(s_srvPub, s_srvNonce, s_salt, s_epoch);

    return true;
}

void BLELNClient::sendEncrypted(const std::string &msg) {
    appendActionToQueue(BLELN_WORKER_ACTION_SEND_MESSAGE, 0,
                        reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
}

bool BLELNClient::isConnected() {
    return (client!= nullptr) && (client->isConnected());
}

void BLELNClient::onPassKeyEntry(NimBLEConnInfo &connInfo) {
    NimBLEDevice::injectPassKey(connInfo, 123456);
}

void BLELNClient::onKeyTxNotify(__attribute__((unused)) NimBLERemoteCharacteristic *ch,
                                __attribute__((unused)) uint8_t *pData,
                                __attribute__((unused)) size_t length,
                                __attribute__((unused)) bool isNotify) {
    NimBLEAttValue v= ch->getValue();

    if(v.size()==0){
        return;
    }

    appendActionToQueue(BLELN_WORKER_ACTION_PROCESS_KEY_RX, 0, v.data(), v.size());
}

void BLELNClient::onDataTxNotify(NimBLERemoteCharacteristic *ch, __attribute__((unused)) uint8_t *pData,
                                 __attribute__((unused)) size_t length,
                                 __attribute__((unused)) bool isNotify) {
    NimBLEAttValue v =ch->getValue();

    if (v.size()==0) {
        return;
    }

    appendActionToQueue(BLELN_WORKER_ACTION_PROCESS_DATA_RX, 0, v.data(), v.size());
}

bool BLELNClient::isScanning() const {
    return scanning;
}

void BLELNClient::appendActionToQueue(uint8_t type, uint16_t conH, const uint8_t *data, size_t dataLen) {
    uint8_t *heapBuf= nullptr;

    if(data!= nullptr) {
        heapBuf = (uint8_t *) malloc(dataLen);
        if (!heapBuf) return;
        memcpy(heapBuf, data, dataLen);
    } else {
        dataLen=0;
    }

    BLELNWorkerAction pkt{conH, type, dataLen, heapBuf};
    if (xQueueSend(workerActionQueue, &pkt, pdMS_TO_TICKS(100)) != pdPASS) {
        free(heapBuf);
    }

}

void BLELNClient::worker() {
    while(runWorker){
        BLELNWorkerAction action{};
        if(xQueueReceive(workerActionQueue, &action, 0)==pdTRUE) {
            if (action.type == BLELN_WORKER_ACTION_REGISTER_CONNECTION) {
                worker_registerConnection(action.connH);
            } else if (action.type == BLELN_WORKER_ACTION_DELETE_CONNECTION) {
                worker_deleteConnection();
            } else if(action.type==BLELN_WORKER_ACTION_SEND_MESSAGE){
                worker_sendMessage(action.d, action.dlen);
            } else if(action.type==BLELN_WORKER_ACTION_PROCESS_KEY_RX){
                worker_processKeyRx(action.d, action.dlen);
            } else if(action.type==BLELN_WORKER_ACTION_PROCESS_DATA_RX){
                worker_processDataRx(action.d, action.dlen);
            }

            free(action.d);
        }


        // Pause task
        if(uxQueueMessagesWaiting(workerActionQueue) > 0){
            vTaskDelay(pdMS_TO_TICKS(5));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void BLELNClient::worker_registerConnection(uint16_t h) {
    if (onConRes) {
        onConRes(true, 0);
    }
    Serial.println("[D] BLELNClient - Client connected");
    connCtx = new BLELNConnCtx(h);

    if(discover()){
        connCtx->setState(BLELNConnCtx::State::WaitingForKey);
    } else {
        Serial.println("[E] BLELNClient - discover failed");
    }
}

void BLELNClient::worker_deleteConnection() {
    if (connCtx) {
        delete connCtx;
        connCtx = nullptr; // Ważne!
    }
}

void BLELNClient::worker_sendMessage(uint8_t *data, size_t dataLen) {
    std::string msg(reinterpret_cast<char*>(data), dataLen);
    std::string encMsg;

    if(connCtx!= nullptr and connCtx->getSessionEnc()->encryptMessage(msg, encMsg)){
        chDataToSer->writeValue(encMsg, false);
    }
}

void BLELNClient::worker_processKeyRx(uint8_t *data, size_t dataLen) {
    if(connCtx!= nullptr){
        if(connCtx->getState()==BLELNConnCtx::State::WaitingForKey){
            Serial.println("Received servers key");
            if(handshake(data, dataLen)){
                connCtx->setState(BLELNConnCtx::State::WaitingForCert);
            } else {
                Serial.println("[E] BLELNClient - handshake failed");
                disconnect(BLE_ERR_AUTH_FAIL);
                connCtx->setState(BLELNConnCtx::State::AuthFailed);
            }
        } else if(connCtx->getState()==BLELNConnCtx::State::WaitingForCert){
            std::string plainKeyMsg;
            if (connCtx->getSessionEnc()->decryptMessage(data, dataLen, plainKeyMsg)) {
                StringList parts= splitCsvRespectingQuotes(plainKeyMsg);
                if(parts[0]==BLELN_MSG_TITLE_CERT and parts.size()==3){

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
                        Serial.println("[E] BLELNClient - WaitingForCert - invalid cert");
                    }
                } else {
                    Serial.println("[E] BLELNClient - WaitingForCert - wrong message");
                    disconnect(BLE_ERR_AUTH_FAIL);
                    connCtx->setState(BLELNConnCtx::State::AuthFailed);
                }
            } else {
                disconnect(BLE_ERR_AUTH_FAIL);
                connCtx->setState(BLELNConnCtx::State::AuthFailed);
            }
        } else if(connCtx->getState()==BLELNConnCtx::State::ChallengeResponseCli) {
            std::string plainKeyMsg;
            if (connCtx->getSessionEnc()->decryptMessage(data, dataLen, plainKeyMsg)) {
                StringList parts = splitCsvRespectingQuotes(plainKeyMsg);
                if (parts[0] == BLELN_MSG_TITLE_CHALLENGE_RESPONSE_NONCE and parts.size() == 2) {
                    sendChallengeNonceSign(connCtx, parts[1]);
                    connCtx->setState(BLELNConnCtx::State::ChallengeResponseSer);
                } else {
                    disconnect(BLE_ERR_AUTH_FAIL);
                    connCtx->setState(BLELNConnCtx::State::AuthFailed);
                }
            } else {
                disconnect(BLE_ERR_AUTH_FAIL);
                connCtx->setState(BLELNConnCtx::State::AuthFailed);
            }
        } else if(connCtx->getState()==BLELNConnCtx::State::ChallengeResponseSer) {
            std::string plainKeyMsg;
            if (connCtx->getSessionEnc()->decryptMessage(data, dataLen, plainKeyMsg)) {
                StringList parts = splitCsvRespectingQuotes(plainKeyMsg);
                if (parts[0] == BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW and parts.size() == 2) {
                    uint8_t nonceSign[BLELN_NONCE_SIGN_LEN];
                    Encryption::base64Decode(parts[1], nonceSign, BLELN_NONCE_SIGN_LEN);
                    if(connCtx->verifyChallengeResponseAnswer(nonceSign)){
                        std::string msg=BLELN_MSG_TITLE_AUTH_OK;
                        msg.append(",1");
                        std::string encMsg;
                        if(connCtx->getSessionEnc()->encryptMessage(msg, encMsg)) {
                            connCtx->setState(BLELNConnCtx::State::Authorised);
                            Serial.println("[D] BLELNClient - auth success");
                            Serial.printf("[D] BLELNClient - client %d live for %d ms\r\n", connCtx->getHandle(), connCtx->getTimeOfLife());
                            chKeyToSer->writeValue(encMsg, true);
                        } else {
                            Serial.println("[E] BLELNClient - failed encrypting cert msg");
                        }
                    } else {
                        Serial.println("[E] BLELNClient - ChallengeResponseSeri - invalid sign");
                        disconnect(BLE_ERR_AUTH_FAIL);
                        connCtx->setState(BLELNConnCtx::State::AuthFailed);
                    }
                } else {
                    disconnect(BLE_ERR_AUTH_FAIL);
                    connCtx->setState(BLELNConnCtx::State::AuthFailed);
                }
            } else {
                disconnect(BLE_ERR_AUTH_FAIL);
                connCtx->setState(BLELNConnCtx::State::AuthFailed);
            }
        }
    }
}

void BLELNClient::worker_processDataRx(uint8_t *data, size_t dataLen) {
    if(connCtx!= nullptr and connCtx->getSessionEnc()->getSessionId() != 0) {
        if(connCtx->getState()==BLELNConnCtx::State::Authorised) {
            if (dataLen >= 4 + 12 + 16) {
                std::string plain;
                if (connCtx->getSessionEnc()->decryptMessage(data, dataLen, plain)) {
                    if (onMsgRx) {
                        onMsgRx(plain);
                    }
                }
            }
        } else {
            disconnect(BLE_ERR_CONN_REJ_SECURITY);
        }
    }
}

void BLELNClient::onDisconnect(NimBLEClient *pClient, int reason) {
    appendActionToQueue(BLELN_WORKER_ACTION_DELETE_CONNECTION, pClient->getConnHandle(), nullptr, 0);
}

void BLELNClient::onConnect(NimBLEClient *pClient) {
    appendActionToQueue(BLELN_WORKER_ACTION_REGISTER_CONNECTION, pClient->getConnHandle(), nullptr, 0);
}

void BLELNClient::onConnectFail(NimBLEClient *pClient, int reason) {
    appendActionToQueue(BLELN_WORKER_ACTION_DELETE_CONNECTION, pClient->getConnHandle(), nullptr, 0);

    if(onConRes)
        onConRes(false, reason);
}


void BLELNClient::disconnect(uint8_t reason) {
    Serial.printf("[D] BLELNClient - disconnected, reason: %d\r\n", reason);
    if(chKeyToCli) chKeyToCli->unsubscribe();
    if(chDataToCli) chDataToCli->unsubscribe();

    svc= nullptr;
    chKeyToCli = nullptr;
    chKeyToSer = nullptr;
    chDataToCli  = nullptr;
    chDataToSer  = nullptr;

    if(client) {
        client->disconnect();
    }
}

/*** Connection context not protected! */
void BLELNClient::sendChallengeNonceSign(BLELNConnCtx *cx, const std::string &nonceB64) {
    uint8_t nonceRaw[BLELN_TEST_NONCE_LEN];             // Servers nonce raw bytes
    uint8_t friendsNonceSign[BLELN_NONCE_SIGN_LEN];     // Servers nonce sing I have created

    // Sign nonce
    Encryption::base64Decode(nonceB64, nonceRaw, BLELN_TEST_NONCE_LEN);
    authStore.signData(nonceRaw, BLELN_TEST_NONCE_LEN, friendsNonceSign);

    // Create clients nonce
    cx->generateTestNonce();
    std::string myNonceB64= cx->getTestNonceBase64();

    // Create BLE message
    std::string msg= BLELN_MSG_TITLE_CHALLENGE_RESPONSE_ANSW_AND_NONCE;
    std::string friendsNonceSignB64= Encryption::base64Encode(friendsNonceSign, BLELN_NONCE_SIGN_LEN);
    msg.append(",").append(friendsNonceSignB64);
    msg.append(",").append(myNonceB64);

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToSer->writeValue(encMsg, true);
    } else {
        Serial.println("[E] BLELNClient - failed encrypting cert msg");
    }
}

/*** Connection context not protected! */
void BLELNClient::sendCertToServer(BLELNConnCtx *cx) {
    std::string msg=BLELN_MSG_TITLE_CERT;
    msg.append(",").append(authStore.getSignedCert());

    std::string encMsg;
    if(cx->getSessionEnc()->encryptMessage(msg, encMsg)) {
        chKeyToSer->writeValue(encMsg, true);
    } else {
        Serial.println("[E] BLELNClient - failed encrypting cert msg");
    }
}





