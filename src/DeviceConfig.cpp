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
#include "Arduino.h"
#include "DeviceConfig.h"

DeviceConfig::DeviceConfig() {
    picklock= nullptr;
    uid= nullptr;
    psk= nullptr;
    ssid= nullptr;
    timezone= nullptr;

    setPicklock("");
    setUid("");
    setPsk("");
    setSsid("");
    setTimezone("");
}

char *DeviceConfig::getSsid() const {
    return ssid;
}

char *DeviceConfig::getPsk() const {
    return psk;
}

char *DeviceConfig::getUid() const {
    return uid;
}

char *DeviceConfig::getPicklock() const {
    return picklock;
}

char *DeviceConfig::getTimezone() const {
    return timezone;
}

void DeviceConfig::setSsid(const char *v) {
    delete[] ssid;

    this->ssid = new char[strlen(v)+1];
    strcpy(this->ssid, v);
}

void DeviceConfig::setPsk(const char *v) {
    delete[] psk;

    this->psk = new char[strlen(v)+1];
    strcpy(this->psk, v);
}

void DeviceConfig::setUid(const char *v) {
    delete[] uid;

    this->uid = new char[strlen(v)+1];
    strcpy(this->uid, v);
}

void DeviceConfig::setPicklock(const char *v) {
    delete[] picklock;

    this->picklock = new char[strlen(v)+1];
    strcpy(this->picklock, v);
}

void DeviceConfig::setTimezone(const char *v) {
    delete[] timezone;

    this->timezone = new char[strlen(v)+1];
    strcpy(this->timezone, v);
}