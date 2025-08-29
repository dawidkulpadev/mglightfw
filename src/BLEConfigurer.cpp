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

#include "BLEConfigurer.h"


bool BLEConfigurer::start(Preferences *preferences, uint8_t *mac, DeviceConfig *deviceConfig, PWMLed *sunLed){
    this->pwmLed= sunLed;
    prefs= preferences;
    deviceConnected= false;
    NimBLEDevice::init("MioGiapicco Light");
    NimBLEDevice::setMTU(247);

    NimBLEDevice::setDeviceName("MioGiapicco Light");

    srv = NimBLEDevice::createServer();
    srv->setCallbacks(this);

    auto* svc = srv->createService(BLE_SERVICE_UUID);

    ch_wifiScanRes = svc->createCharacteristic(BLE_CHAR_UUID_WIFI_SCAN_RES,
                                             NIMBLE_PROPERTY::NOTIFY |
                                             NIMBLE_PROPERTY::WRITE |
                                             NIMBLE_PROPERTY::READ);
    ch_wifiSSID= svc->createCharacteristic(CHARACTERISTIC_UUID_WIFI_SSID,
                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    ch_wifiPSK= svc->createCharacteristic(CHARACTERISTIC_UUID_WIFI_PSK,
                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    ch_uid= svc->createCharacteristic(CHARACTERISTIC_UUID_UID,
                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    ch_picklock= svc->createCharacteristic(CHARACTERISTIC_UUID_PICKLOCK,
                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    ch_mac= svc->createCharacteristic(CHARACTERISTIC_UUID_MAC,
                                      NIMBLE_PROPERTY::READ);
    ch_setFlag= svc->createCharacteristic(CHARACTERISTIC_UUID_SET_FLAG,
                                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    ch_timezone= svc->createCharacteristic(CHARACTERISTIC_UUID_TIMEZONE,
                                           NIMBLE_PROPERTY::READ |
                                           NIMBLE_PROPERTY::WRITE);

    ch_wifiScanRes->setCallbacks(this);
    ch_setFlag->setCallbacks(this);

    svc->start();

    auto* adv = NimBLEDevice::getAdvertising();
    adv->setName("MioGiapicco Light");
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->enableScanResponse(true);
    adv->setPreferredParams(0x06, 0x12);
    NimBLEDevice::startAdvertising();

    Serial.println("BLE started!");

    char str_mac[28];
    sprintf(str_mac, "%02X%02X%02X%02X%02X%02X\0", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println(("Picklock: len= "+std::to_string(strlen(str_mac))+" "+std::string(str_mac)).c_str());
    ch_mac->setValue(std::string(str_mac));


    ch_wifiSSID->setValue(std::string(deviceConfig->getSsid()));
    ch_wifiPSK->setValue(std::string(deviceConfig->getPsk()));
    ch_uid->setValue(std::string(deviceConfig->getUid()));
    ch_picklock->setValue(std::string(deviceConfig->getPicklock()));
    ch_timezone->setValue(std::string(deviceConfig->getTimezone()));

    ch_wifiScanRes->setValue("");

  return true;
}

void BLEConfigurer::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo){
  if(pCharacteristic->getUUID().toString() == CHARACTERISTIC_UUID_SET_FLAG){
    std::string wifiSSID= ch_wifiSSID->getValue();
    std::string wifiPSK= ch_wifiPSK->getValue();
    std::string picklock= ch_picklock->getValue();
    std::string uid= ch_uid->getValue();
    std::string timezone= ch_timezone->getValue();

    DeviceConfig deviceConfig;
    deviceConfig.setSsid(wifiSSID.c_str());
    deviceConfig.setPsk(wifiPSK.c_str());
    deviceConfig.setUid(uid.c_str());
    deviceConfig.setPicklock(picklock.c_str());
    deviceConfig.setTimezone(timezone.c_str());
    ConfigManager::writeWifi(prefs, &deviceConfig);

    delay(2000);
    ch_setFlag->setValue("0");
    ch_setFlag->notify();
    Serial.println("Configuration received");
    Serial.printf("Picklock: %s\r\n", picklock.c_str());

    configReceived = true;
    configreceivedAt = millis();
  }
}

void BLEConfigurer::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo){
  deviceConnected= true;
  Serial.println("BLE device connected");
}
void BLEConfigurer::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason){
  deviceConnected= false;
  NimBLEDevice::startAdvertising();
  Serial.println("BLE device disconnected");
}

bool BLEConfigurer::isConnected() const{
  return deviceConnected;
}

void BLEConfigurer::updateWiFiScanResults(const std::string &scanRes) {
    ch_wifiScanRes->setValue(scanRes);
    ch_wifiScanRes->notify();
    Serial.println("WiFi scan result updated");
}

bool BLEConfigurer::isConfigReceived() const {
    return configReceived;
}

uint32_t BLEConfigurer::getConfigReceivedAt() const {
    return configreceivedAt;
}
