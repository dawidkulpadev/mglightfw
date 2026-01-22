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

#include "ConfigManager.h"

bool ConfigManager::readWifi(Preferences *prefs, DeviceConfig *config) {
    config->setSsid("dlink3");
    config->setPsk("sikakama2");
    config->setUid("2");
    config->setPicklock("test");
    config->setTimezone("Europe/Warsaw");
    return true;

    if( !prefs->isKey(CONFIGMANAGER_KEY_SSID) or
        !prefs->isKey(CONFIGMANAGER_KEY_PSK) or
        !prefs->isKey(CONFIGMANAGER_KEY_PICKLOCK) or
        !prefs->isKey(CONFIGMANAGER_KEY_UID) or
        !prefs->isKey(CONFIGMANAGER_KEY_TIMEZONE)){
        return false;
    }

    char buf[256];

    // Read SSID
    prefs->getString(CONFIGMANAGER_KEY_SSID, buf, 256);
    config->setSsid(buf);

    // Read psk
    prefs->getString(CONFIGMANAGER_KEY_PSK, buf, 256);
    config->setPsk(buf);

    // Read UId
    prefs->getString(CONFIGMANAGER_KEY_UID, buf, 256);
    config->setUid(buf);

    // Read picklock
    prefs->getString(CONFIGMANAGER_KEY_PICKLOCK, buf, 256);
    config->setPicklock(buf);

    // Read timezone
    prefs->getString(CONFIGMANAGER_KEY_TIMEZONE, buf, 256);
    config->setTimezone(buf);

    return true;
}

bool ConfigManager::writeWifi(Preferences *prefs, DeviceConfig *config) {
    Serial.println(std::to_string(strlen(config->getSsid())).c_str());
    Serial.println(std::to_string(prefs->putString(CONFIGMANAGER_KEY_SSID, config->getSsid())).c_str());
    prefs->putString(CONFIGMANAGER_KEY_PSK, config->getPsk());
    prefs->putString(CONFIGMANAGER_KEY_UID, config->getUid());
    prefs->putString(CONFIGMANAGER_KEY_PICKLOCK, config->getPicklock());
    prefs->putString(CONFIGMANAGER_KEY_TIMEZONE, config->getTimezone());

    return true;
}

bool ConfigManager::writeDay(Preferences *prefs, Day *day){
    prefs->putString(CONFIGMANAGER_KEY_DLI, std::to_string(day->getDli()).c_str());
    prefs->putString(CONFIGMANAGER_KEY_DS, std::to_string(day->getDs()).c_str());
    prefs->putString(CONFIGMANAGER_KEY_DE, std::to_string(day->getDe()).c_str());
    prefs->putString(CONFIGMANAGER_KEY_SSD, std::to_string(day->getSsd()).c_str());
    prefs->putString(CONFIGMANAGER_KEY_SRD, std::to_string(day->getSrd()).c_str());

    return true;
}

bool ConfigManager::readDay(Preferences *prefs, Day *day){
    if(!prefs->isKey(CONFIGMANAGER_KEY_DLI) or
       !prefs->isKey(CONFIGMANAGER_KEY_DS) or
       !prefs->isKey(CONFIGMANAGER_KEY_DE) or
       !prefs->isKey(CONFIGMANAGER_KEY_SSD) or
       !prefs->isKey(CONFIGMANAGER_KEY_SRD)){
        return false;
    }

    char buf[32];

    // Read DLI
    prefs->getString(CONFIGMANAGER_KEY_DLI, buf, 32);
    day->setDli(std::stoi(buf));

    // Read DS
    prefs->getString(CONFIGMANAGER_KEY_DS, buf, 32);
    day->setDs(std::stoi(buf));

    // Read DE
    prefs->getString(CONFIGMANAGER_KEY_DE, buf, 32);
    day->setDe(std::stoi(buf));

    // Read SSD
    prefs->getString(CONFIGMANAGER_KEY_SSD, buf, 32);
    day->setSsd(std::stoi(buf));

    // Read SRD
    prefs->getString(CONFIGMANAGER_KEY_SRD, buf, 32);
    day->setSrd(std::stoi(buf));

    return true;
}

bool ConfigManager::clearWifiConfig(Preferences *prefs) {
    prefs->remove(CONFIGMANAGER_KEY_SSID);
    prefs->remove(CONFIGMANAGER_KEY_PSK);
    prefs->remove(CONFIGMANAGER_KEY_UID);
    prefs->remove(CONFIGMANAGER_KEY_PICKLOCK);
    prefs->remove(CONFIGMANAGER_KEY_TIMEZONE);
    return true;
}

