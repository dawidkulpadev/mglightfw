//
// Created by dkulpa on 7.12.2025.
//

#ifndef MGLIGHTFW_BLELNCLIENT_H
#define MGLIGHTFW_BLELNCLIENT_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "BLELNSessionEnc.h"

struct RxClientPacket {
    size_t   len;
    uint8_t* buf;    // malloc/free
};

class BLELNClient : public NimBLEScanCallbacks, public NimBLEClientCallbacks{
public:
    void start(const std::string &name, std::function<void(const std::string&)> onServerResponse);
    void stop();
    void startServerSearch(uint32_t durationMs, const std::string &serverUUID, const std::function<void(const NimBLEAdvertisedDevice *advertisedDevice)>& onResult);
    void beginConnect(const NimBLEAdvertisedDevice *advertisedDevice, const std::function<void(bool, int)> &onConnectResult);
    bool sendEncrypted(const std::string& msg);
    void disconnect();

    bool isScanning() const;
    bool isConnected();
    bool hasDiscoveredClient();

    void onConnect(NimBLEClient* pClient) override;

    void onConnectFail(NimBLEClient *pClient, int reason) override;

    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;

    void rxWorker();
    void appendToQueue(const std::string &m);

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override;

    bool discover();
    bool handshake();


private:
    void onKeyExNotifyClb(__attribute__((unused)) NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length,
                          __attribute__((unused)) bool isNotify);
    void onServerResponse(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, __attribute__((unused)) uint8_t* pData,
                          __attribute__((unused)) size_t length, __attribute__((unused)) bool isNotify);

    NimBLEClient* client=nullptr;
    NimBLERemoteService* svc=nullptr;
    NimBLERemoteCharacteristic *chKeyExTx=nullptr,*chKeyExRx=nullptr,*chDataTx=nullptr,*chDataRx=nullptr;

    bool scanning = false;
    std::function<void(const NimBLEAdvertisedDevice *advertisedDevice)> onScanResult;
    std::string searchedUUID;

    std::function<void(const std::string&)> onMsgRx;
    std::function<void(bool, int)> onConRes;
    bool runRxWorker=false;

    QueueHandle_t g_rxQueue;

    volatile bool g_keyexReady = false;
    std::string   g_keyexPayload;

    BLELNSessionEnc sessionEnc;
};


#endif //MGLIGHTFW_BLELNCLIENT_H
