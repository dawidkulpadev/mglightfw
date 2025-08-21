/**
    MioGiapicco Light Firmware - Firmware for Light Device of MioGiapicco system
    Copyright (C) 2023  Dawid Kulpa

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


#ifndef BLEMANAGER_H
#define BLEMANAGER_H

#define SERVICE_UUID                    "952cb13b-57fa-4885-a445-57d1f17328fd"
#define BLE_CHAR_UUID_WIFI_SCAN_RES     "ef7cb0fc-53a4-4062-bb0e-25443e3a1f5d"
#define CHARACTERISTIC_UUID_WIFI_SSID   "345ac506-c96e-45c6-a418-56a2ef2d6072"
#define CHARACTERISTIC_UUID_WIFI_PSK    "b675ddff-679e-458d-9960-939d8bb03572"
#define CHARACTERISTIC_UUID_UID         "566f9eb0-a95e-4c18-bc45-79bd396389af"
#define CHARACTERISTIC_UUID_PICKLOCK    "f6ffba4e-eea1-4728-8b1a-7789f9a22da8"
#define CHARACTERISTIC_UUID_MAC         "c0cd497d-6987-41fa-9b6d-ef2e2a94e04a"
#define CHARACTERISTIC_UUID_SET_FLAG    "e34fc92f-7565-403b-9528-35b4650596fc"
#define CHARACTERISTIC_UUID_TIMEZONE    "e00758dd-7c07-42fd-8699-423b73fcb4ce"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLECharacteristic.h"
#include "BLE2902.h"
#include "Ticker.h"

#include "Arduino.h"
#include "ConfigManager.h"
#include "DeviceConfig.h"
#include "PWMLed.h"

class BLEConfigurer : public BLECharacteristicCallbacks, public BLEServerCallbacks {
  public:
    bool start(uint8_t *mac, DeviceConfig *deviceConfig, PWMLed *sunLed);
    void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) override;
    void onConnect(BLEServer* s) override;
    void onDisconnect(BLEServer* s) override;
    bool isConnected() const;
    void updateWiFiScanResults(std::string scanRes);


  private:
    bool deviceConnected;

    BLEServer *server;
    BLEService *service;

    BLECharacteristic *ch_wifiScanRes;
    BLECharacteristic *ch_wifiSSID;
    BLECharacteristic *ch_wifiPSK;
    BLECharacteristic *ch_uid;
    BLECharacteristic *ch_picklock;
    BLECharacteristic *ch_mac;
    BLECharacteristic *ch_setFlag;
    BLECharacteristic *ch_timezone;

    PWMLed *pwmLed;
};


#endif //BLEMANAGER_H