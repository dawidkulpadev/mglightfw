//
// Created by dkulpa on 7.12.2025.
//

#include "BLELNClient.h"

#include <utility>
#include "BLELNBase.h"

void BLELNClient::start(const std::string &name, std::function<void(const std::string&)> onServerResponse) {
    NimBLEDevice::init(name);
    NimBLEDevice::setSecurityAuth(true,true,true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setMTU(247);
    g_rxQueue = xQueueCreate(20, sizeof(RxClientPacket));
    onMsgRx= std::move(onServerResponse);
    runRxWorker= true;
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

    runRxWorker= false;
    if(g_rxQueue)
        xQueueReset(g_rxQueue);

    if(chKeyExTx)
        chKeyExTx->unsubscribe(true);
    if(chDataTx)
        chDataTx ->unsubscribe(true);

    if(client!= nullptr){
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        client= nullptr;
    }

    chKeyExTx = nullptr;
    chKeyExRx = nullptr;
    chDataTx  = nullptr;
    chDataRx  = nullptr;
    svc       = nullptr;

    onMsgRx = nullptr;

    g_keyexPayload.clear();
    g_keyexReady = false;

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
    chKeyExTx = s->getCharacteristic(BLELNBase::KEYEX_TX_UUID);
    chKeyExRx = s->getCharacteristic(BLELNBase::KEYEX_RX_UUID);
    chDataTx  = s->getCharacteristic(BLELNBase::DATA_TX_UUID);
    chDataRx  = s->getCharacteristic(BLELNBase::DATA_RX_UUID);

    if(chKeyExTx && chKeyExRx && chDataTx && chDataRx) {
        chKeyExTx->subscribe(true,
                             [this](NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                                    bool isNotify) {
                                 this->onKeyExNotifyClb(pBLERemoteCharacteristic, pData, length, isNotify);
                             });
        chDataTx->subscribe(true,
                            [this](NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                                   bool isNotify) {
                                this->onServerResponse(pBLERemoteCharacteristic, pData, length, isNotify);
                            });
    }

    return chKeyExTx && chKeyExRx && chDataTx && chDataRx;
}

// TODO: Make nonblocking, rename "tryHandshake"
bool BLELNClient::handshake() {
    // KEYEX_TX: [ver=1][epoch:4][salt:32][srvPub:65][srvNonce:12]
    uint32_t t0 = millis();

    // Max wait: 5s
    Serial.println("Waiting for g_keyexReady");
    while (!g_keyexReady && millis() - t0 < 5000) {
        delay(10);
    }

    if (!g_keyexReady) {
        disconnect();
        Serial.println("[HX] timeout waiting KEYEX_TX notify");
        return false;
    } else {
        Serial.println("KeyEx received");
    }

    const std::string &v = g_keyexPayload;

    if (v.size()!=1+4+32+65+12 || (uint8_t)v[0]!=1) {
        Serial.printf("[HX] bad keyex len=%u\n",(unsigned)v.size());
        return false;
    }

    uint32_t s_epoch = 0;
    uint8_t  s_salt[32], s_srvPub[65], s_srvNonce[12];

    memcpy(&s_epoch,  &v[1], 4);
    memcpy(s_salt,    &v[1+4], 32);
    memcpy(s_srvPub,  &v[1+4+32], 65);
    memcpy(s_srvNonce,&v[1+4+32+65], 12);

    sessionEnc.makeMyKeys();

    // [ver=1][cliPub:65][cliNonce:12]
    std::string tx;
    tx.push_back(1);
    tx.append((const char*)sessionEnc.getMyPub(),65);
    tx.append((const char*)sessionEnc.getMyNonce(),12);

    if(!chKeyExRx->writeValue(tx,true)){
        Serial.println("[HX] write fail");
        return false;
    }

    sessionEnc.deriveFriendsKey(s_srvPub, s_srvNonce, s_salt, s_epoch);


    return true;
}

bool BLELNClient::sendEncrypted(const std::string &msg) {
    std::string pkt;
    if(!sessionEnc.encryptAESGCM(msg, pkt)){
        return false;
    }

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

void BLELNClient::onKeyExNotifyClb(__attribute__((unused)) NimBLERemoteCharacteristic *c, uint8_t *pData, size_t length,
                                   __attribute__((unused)) bool isNotify) {
    g_keyexPayload.assign((const char*)pData, length);
    g_keyexReady = true;
}

void BLELNClient::onServerResponse(NimBLERemoteCharacteristic *c, __attribute__((unused)) uint8_t *pData,
                                   __attribute__((unused)) size_t length,
                                   __attribute__((unused)) bool isNotify) {
    const std::string &v = c->getValue();

    if (v.empty()) {
        return;
    }

    appendToQueue(v);
}

bool BLELNClient::isScanning() const {
    return scanning;
}

void BLELNClient::appendToQueue(const std::string &m) {
    auto* heapBuf = (uint8_t*)malloc(m.size());
    if (!heapBuf) return;
    memcpy(heapBuf, m.data(), m.size());

    RxClientPacket pkt{ m.size(), heapBuf };
    if (xQueueSend(g_rxQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
        free(heapBuf);
    }
}

void BLELNClient::rxWorker() {
    for(;;) {
        if(!runRxWorker){
            if (g_rxQueue) {
                RxClientPacket pkt{};
                while (xQueueReceive(g_rxQueue, &pkt, 0) == pdPASS) {
                    free(pkt.buf);
                }
            }
            return;
        }

        if(sessionEnc.getSessionId()!=0) {
            RxClientPacket pkt{};
            if (xQueueReceive(g_rxQueue, &pkt, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (pkt.len >= 4 + 12 + 16) {
                    std::string plain;
                    if(sessionEnc.decryptAESGCM(pkt.buf, pkt.len, plain)) {
                        if (onMsgRx) {
                            onMsgRx(plain);
                        }
                    }
                }

                free(pkt.buf);
            }

            if (uxQueueMessagesWaiting(g_rxQueue) > 0) {
                vTaskDelay(pdMS_TO_TICKS(1));
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void BLELNClient::onDisconnect(NimBLEClient *pClient, int reason) {
    NimBLEClientCallbacks::onDisconnect(pClient, reason);
}

void BLELNClient::onConnect(NimBLEClient *pClient) {
    if(onConRes){
        onConRes(true, 0);
    }
    Serial.println("Client connected");
}

void BLELNClient::onConnectFail(NimBLEClient *pClient, int reason) {
    if(onConRes)
        onConRes(false, reason);
}


void BLELNClient::disconnect() {
    chKeyExTx->unsubscribe();
    chDataTx->unsubscribe();

    svc= nullptr;
    chKeyExTx = nullptr;
    chKeyExRx = nullptr;
    chDataTx  = nullptr;
    chDataRx  = nullptr;

    client->disconnect();
    NimBLEDevice::deleteClient(client);

    g_keyexPayload.clear();
    g_keyexReady = false;
}
