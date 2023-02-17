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

const char* ConfigManager::wcfn = "/wifiConfig.json";
const char* ConfigManager::dcfn = "/dayConfig.json";

void ConfigManager::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount file system");
    }
}

bool ConfigManager::readWifi(DeviceConfig *config) {
    File configFile = LittleFS.open(ConfigManager::wcfn, "r");
    if (!configFile) {
        Serial.println("Failed to open wifi config file");
        return false;
    }

    size_t size = configFile.size();
    if (size > 1024) {
        Serial.println("Wifi config file size is too large");
        return false;
    }

    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    configFile.readBytes(buf.get(), size);

    StaticJsonDocument<200> doc;
    auto error = deserializeJson(doc, buf.get());
    if (error) {
        Serial.println("Failed to parse wifi config file");
        return false;
    }

    config->setSsid(doc["ssid"]);
    config->setPsk(doc["psk"]);
    config->setUid(doc["uid"]);
    config->setPicklock(doc["picklock"]);
    config->setTimezone(doc["timezone"]);

    Serial.print(config->getUid());
    Serial.println(config->getPicklock());

    return true;
}

bool ConfigManager::writeWifi(DeviceConfig *config) {
    StaticJsonDocument<200> doc;
    doc["ssid"] = config->getSsid();
    doc["psk"] = config->getPsk();
    doc["uid"] = config->getUid();
    doc["picklock"] = config->getPicklock();
    doc["timezone"] = config->getTimezone();

    File configFile = LittleFS.open(ConfigManager::wcfn, "w");
    if (!configFile) {
        Serial.println("Failed to open wifi config file for writing");
        return false;
    }

    serializeJson(doc, configFile);
    return true;
}

bool ConfigManager::writeDay(Day *day){
    StaticJsonDocument<200> doc;
    
    doc["dli"] = std::to_string(day->getDli());
    doc["ds"] = std::to_string(day->getDs());
    doc["de"] = std::to_string(day->getDe());
    doc["ssd"] = std::to_string(day->getSsd());
    doc["srd"] = std::to_string(day->getSrd());

    

    File configFile = LittleFS.open(ConfigManager::dcfn, "w");
    if (!configFile) {
        Serial.println("Failed to open Day config file for writing");
        return false;
    }

    serializeJson(doc, configFile);
    return true;
}

bool ConfigManager::readDay(Day *day){
    File configFile = LittleFS.open(ConfigManager::dcfn, "r");
    if (!configFile) {
        Serial.println("Failed to open Day config file");
        return false;
    }

    size_t size = configFile.size();
    if (size > 1024) {
        Serial.println("Day config file size is too large");
        return false;
    }

    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    configFile.readBytes(buf.get(), size);

    StaticJsonDocument<200> doc;
    auto error = deserializeJson(doc, buf.get());
    if (error) {
        Serial.println("Failed to parse Day config file");
        return false;
    }

    day->setDli(atoi(doc["dli"]));
    day->setDs(atoi(doc["ds"]));
    day->setDe(atoi(doc["de"]));
    day->setSsd(atoi(doc["ssd"]));
    day->setSrd(atoi(doc["srd"]));

    return true;
}

bool ConfigManager::clearWifiConfig() {
    return LittleFS.remove(ConfigManager::wcfn);
}
