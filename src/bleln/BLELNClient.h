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
    bool sendEncrypted(const std::string& msg);
    void disconnect(uint8_t reason=BLE_ERR_REM_USER_CONN_TERM);

    bool isScanning() const;
    bool isConnected();

    void onConnect(NimBLEClient* pClient) override;
    void onConnectFail(NimBLEClient *pClient, int reason) override;
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;

    void rxWorker();
    static void appendToQueue(const uint8_t *m, size_t mlen, QueueHandle_t *queue);

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override;
private:
    void sendCertToServer(BLELNConnCtx *cx);
    void sendChallengeNonce(BLELNConnCtx *cx);
    void sendChallengeNonceSign(BLELNConnCtx *cx, const std::string &nonceB64);
    bool discover();
    bool handshake(BLELNConnCtx *cx, uint8_t *v, size_t vlen);

    void onKeyTxNotify(__attribute__((unused)) NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length,
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

    QueueHandle_t dataRxQueue;
    QueueHandle_t keyRxQueue;

    BLELNConnCtx *connCtx= nullptr;
    BLELNAuthentication authStore;
    SemaphoreHandle_t connMtx = nullptr;

    bool runWorker=false;
};


#endif //MGLIGHTFW_BLELNCLIENT_H
