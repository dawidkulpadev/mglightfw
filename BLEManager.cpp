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

#include "BLEManager.h"


bool BLEManager::start(uint8_t *mac, DeviceConfig *deviceConfig){
  deviceConnected= false;
  BLEDevice::init("MioGiapicco Light");

  server = BLEDevice::createServer();
  server->setCallbacks(this);
  service = server->createService(BLEUUID(SERVICE_UUID), 30);

  ch_wifiSSID = service->createCharacteristic(CHARACTERISTIC_UUID_WIFI_SSID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
                                       
  ch_wifiPSK = service->createCharacteristic(CHARACTERISTIC_UUID_WIFI_PSK,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  ch_uid = service->createCharacteristic(CHARACTERISTIC_UUID_UID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  ch_picklock = service->createCharacteristic(CHARACTERISTIC_UUID_PICKLOCK,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  ch_mac = service->createCharacteristic(CHARACTERISTIC_UUID_MAC,
                                         BLECharacteristic::PROPERTY_READ
                                       );

  ch_timezone = service->createCharacteristic(CHARACTERISTIC_UUID_TIMEZONE,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  ch_setFlag = service->createCharacteristic(
                                         CHARACTERISTIC_UUID_SET_FLAG,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE |
                                         BLECharacteristic::PROPERTY_NOTIFY
                                       );
  ch_setFlag->addDescriptor(new BLE2902());
  ch_setFlag->setCallbacks(this);

  if(deviceConfig->getSsid()!=nullptr)
    ch_wifiSSID->setValue(std::string(deviceConfig->getSsid()));
    if(deviceConfig->getPsk()!=nullptr)
    ch_wifiPSK->setValue(std::string(deviceConfig->getPsk()));
  if(deviceConfig->getUid()!=nullptr)
    ch_uid->setValue(std::string(deviceConfig->getUid()));
  if(deviceConfig->getPicklock()!=nullptr)
    ch_picklock->setValue(std::string(deviceConfig->getPicklock()));
  if(deviceConfig->getTimezone()!=nullptr)
    ch_timezone->setValue(std::string(deviceConfig->getTimezone()));

  char str_mac[24];
  sprintf(str_mac, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  ch_mac->setValue(str_mac);

  service->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); 
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE started!");

  return true;
}

void BLEManager::onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param){

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
    ConfigManager::writeWifi(&deviceConfig);

    delay(2000);
    ch_setFlag->setValue("0");
    ch_setFlag->notify();
    Serial.println("Configuration received");
  }
}

void BLEManager::onConnect(BLEServer* s){
  deviceConnected= true;
  Serial.println("BLE device connected");
}
void BLEManager::onDisconnect(BLEServer* s){
  deviceConnected= false;
  Serial.println("BLE device disconnected");
}

bool BLEManager::isConnected(){
  return deviceConnected;
}
