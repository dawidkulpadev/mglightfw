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

#include "WiFiManager.h"

WiFiManager::WiFiManager() {
  WiFi.macAddress(mac);
}

void WiFiManager::initNormalMode(const char* ssid, const char* psk) {
  WiFi.begin(ssid, psk);
  Serial.printf("WiFi: %s, %s\n", ssid, psk);
  Serial.printf("MAC address: %x %x %x %x %x %x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

uint8_t* WiFiManager::getMAC(){
  return mac;
}


