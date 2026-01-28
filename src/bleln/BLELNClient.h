//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_BLELNCLIENT_H
#define MGLIGHTFW_BLELNCLIENT_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "BLELNSessionEnc.h"
#include "BLELNConnCtx.h"
#include "BLELNAuthentication.h"


class BLELNClient : public NimBLEScanCallbacks, public NimBLEClientCallbacks{
public:
    void start(const std::string &name, std::function<void(const std::string&)> onServerResponse);
    void stop();
    void startServerSearch(uint32_t durationMs, const std::string &serverUUID, const std::function<void(const NimBLEAdvertisedDevice *advertisedDevice)>& onResult);
    void beginConnect(const NimBLEAdvertisedDevice *advertisedDevice, const std::function<void(bool, int)> &onConnectResult);
    void sendEncrypted(const std::string& msg);
    void disconnect(uint8_t reason=BLE_ERR_REM_USER_CONN_TERM);

    bool isScanning() const;
    bool isConnected();

    void onConnect(NimBLEClient* pClient) override;
    void onConnectFail(NimBLEClient *pClient, int reason) override;
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;

    void worker();
    void appendActionToQueue(uint8_t type, uint16_t conH, const uint8_t *data, size_t dataLen);

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override;
private:
    void worker_registerConnection(uint16_t h);
    void worker_deleteConnection();
    void worker_sendMessage(uint8_t *data, size_t dataLen);
    void worker_processKeyRx(uint8_t *data, size_t dataLen);
    void worker_processDataRx(uint8_t *data, size_t dataLen);

    void sendCertToServer(BLELNConnCtx *cx);
    void sendChallengeNonceSign(BLELNConnCtx *cx, const std::string &nonceB64);
    bool discover();
    bool handshake(uint8_t *v, size_t vlen);

    void onKeyTxNotify(__attribute__((unused)) NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                       __attribute__((unused)) uint8_t* pData, __attribute__((unused)) __attribute__((unused)) size_t length,
                       __attribute__((unused)) bool isNotify);
    void onDataTxNotify(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, __attribute__((unused)) uint8_t* pData,
                        __attribute__((unused)) size_t length, __attribute__((unused)) bool isNotify);

    NimBLEClient* client=nullptr;
    NimBLERemoteService* svc=nullptr;
    NimBLERemoteCharacteristic *chKeyToCli=nullptr,*chKeyToSer=nullptr,*chDataToCli=nullptr,*chDataToSer=nullptr;

    bool scanning = false;
    std::function<void(const NimBLEAdvertisedDevice *advertisedDevice)> onScanResult;
    std::string searchedUUID;

    std::function<void(const std::string&)> onMsgRx;
    std::function<void(bool, int)> onConRes;

    QueueHandle_t workerActionQueue;

    BLELNConnCtx *connCtx= nullptr;
    BLELNAuthentication authStore;

    bool runWorker=false;
    TaskHandle_t workerTaskHandle = nullptr;
};


#endif //MGLIGHTFW_BLELNCLIENT_H
