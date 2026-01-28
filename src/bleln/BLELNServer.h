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

#ifndef MGLIGHTFW_G2_BLELNSERVER_H
#define MGLIGHTFW_G2_BLELNSERVER_H

#include "Arduino.h"
#include "BLELNConnCtx.h"
#include "BLELNBase.h"
#include "BLELNAuthentication.h"
#include <NimBLEDevice.h>
#include <Preferences.h>

#include <list>


#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// TODO: Add new clients, deleted clients and send queues




class BLELNServer : public NimBLEScanCallbacks, public NimBLEServerCallbacks{
public:
    // User methods
    void start(Preferences *prefs, const std::string &name, const std::string &uuid);
    void stop();
    void startOtherServerSearch(uint32_t durationMs, const std::string &therUUID, const std::function<void(bool)>& onResult);
    bool getConnContext(uint16_t h, BLELNConnCtx** c);
    bool noClientsConnected();


    bool sendEncrypted(uint16_t h, const std::string& msg);
    bool sendEncryptedToAll(const std::string& msg);

    void setOnMessageReceivedCallback(std::function<void(uint16_t cliH, const std::string& msg)> cb);

    void worker();

private:
    // Intefaces
    std::function<void(uint16_t cliH, const std::string& msg)> onMsgReceived;

    // NimBLE
    NimBLEServer* srv = nullptr;
    NimBLECharacteristic *chKeyToCli=nullptr, *chKeyToSer=nullptr, *chDataToCli=nullptr, *chDataToSer=nullptr;
    NimBLECharacteristicCallbacks* keyTxClb = nullptr;
    NimBLECharacteristicCallbacks* keyRxClb = nullptr;
    NimBLECharacteristicCallbacks* dataRxClb = nullptr;

    // Multithreading
    TaskHandle_t workerTaskHandle = nullptr;
    SemaphoreHandle_t clisMtx = nullptr;
    QueueHandle_t workerActionQueue;
    bool runWorker;

    // Encryption
    uint8_t g_psk_salt[32];
    uint32_t g_epoch = 0;
    uint32_t g_lastRotateMs = 0;

    // BLELN
    BLELNAuthentication authStore;
    std::list<BLELNConnCtx> connCtxs;

    std::string serviceUUID;
    bool scanning = false;
    std::function<void(bool found)> onScanResult;
    std::string searchedUUID;

    unsigned long lastWaterMarkPrint=0;

    // Private methods
    void worker_registerClient(uint16_t h);
    void worker_deleteClient(uint16_t h);
    void worker_processSubscription(uint16_t h);
    void worker_sendMessage(uint16_t h, uint8_t *data, size_t dataLen);
    void worker_processKeyRx(uint16_t h, uint8_t *data, size_t dataLen);
    void worker_processDataRx(uint16_t h, uint8_t *data, size_t dataLen);
    void worker_cleanup();

    bool _sendEncrypted(BLELNConnCtx *cx, const std::string& msg);
    void sendKeyToClient(BLELNConnCtx *cx);
    void sendCertToClient(BLELNConnCtx *cx);
    void sendChallengeNonce(BLELNConnCtx *cx);
    void sendChallengeNonceSign(BLELNConnCtx *cx, uint8_t *sign);
    void disconnectClient(BLELNConnCtx *cx, uint8_t reason=BLE_ERR_REM_USER_CONN_TERM);
    void appendToDataQueue(uint16_t h, const std::string &m);
    void appendToKeyQueue(uint16_t h, const std::string &m);

    // Callbacks
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    void onDataWrite(NimBLECharacteristic* c, NimBLEConnInfo& info);
    void onKeyToSerWrite(NimBLECharacteristic* c, NimBLEConnInfo& info);
    void onKeyToCliSubscribe(__attribute__((unused)) NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue);

    // Callback classes
    class DataRxClb : public NimBLECharacteristicCallbacks{
    public:
        explicit DataRxClb(BLELNServer *parent): parentServer(parent){}
    private:
        BLELNServer *parentServer;
        void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override{
            parentServer->onDataWrite(c, info);
        }
    };

    class KeyRxClb : public NimBLECharacteristicCallbacks {
    public:
        explicit KeyRxClb(BLELNServer *parent): parentServer(parent){}
    private:
        BLELNServer *parentServer;
        void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override{
            parentServer->onKeyToSerWrite(c, info);
        }
    };

    class KeyTxClb : public NimBLECharacteristicCallbacks {
    public:
        explicit KeyTxClb(BLELNServer *parent): parentServer(parent){}
    private:
        BLELNServer *parentServer;
        void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override{
            parentServer->onKeyToCliSubscribe(pCharacteristic, connInfo, subValue);
        }
    };
};


#endif //MGLIGHTFW_G2_BLELNSERVER_H
