//
// Created by dkulpa on 17.08.2025.
//

#ifndef MGLIGHTFW_G2_BLELNCLIENT_H
#define MGLIGHTFW_G2_BLELNCLIENT_H

#include "BLELNBase.h"
#include <NimBLEDevice.h>

class BLELNClient : public NimBLEScanCallbacks, public NimBLEClientCallbacks{
public:
    void start();
    void stop();
    void startServerSearch(uint32_t durationMs, const std::function<void(bool)>& onResult);
    bool sendEncrypted(const std::string& msg, std::function<void(std::string)> onServerResponse);

    bool isScanning() const;
    bool isConnected();
    bool hasDiscoveredClient();

    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override;

    bool discover();
    bool handshake();

private:
    void onKeyExNotifyClb(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void onServerResponse(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

    NimBLEClient* client=nullptr;
    NimBLERemoteService* svc=nullptr;
    NimBLERemoteCharacteristic *chKeyExTx=nullptr,*chKeyExRx=nullptr,*chDataTx=nullptr,*chDataRx=nullptr;

    bool scanning = false;
    std::function<void(bool)> onScanResult;
    std::function<void(std::string)> onMsgRx;

    volatile bool g_keyexReady = false;
    std::string   g_keyexPayload;

    uint16_t s_sid=0;
    uint32_t s_epoch=0;
    uint8_t  s_salt[32];
    uint8_t  s_srvPub[65], s_srvNonce[12];
    uint8_t  s_cliPub[65], s_cliNonce[12];
    uint8_t  s_sessKey_c2s[32], s_sessKey_s2c[32];
    uint32_t s_ctr_c2s=0, s_ctr_s2c=0;


};


#endif //MGLIGHTFW_G2_BLELNCLIENT_H
