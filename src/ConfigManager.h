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

#ifndef UNTITLED_CONFIGMANAGER_H
#define UNTITLED_CONFIGMANAGER_H

#include "DeviceConfig.h"
#include "Day.h"
#include <ArduinoJson.h>

#include "FS.h"
#include <LittleFS.h>

class ConfigManager {
public:
    static void init();
    static bool readWifi(DeviceConfig *config);
    static bool writeWifi(DeviceConfig *config);
    static bool writeDay(Day *day);
    static bool readDay(Day *day);
    static bool clearWifiConfig();

    static bool readCaStore(unsigned char *c) ;

private:
    static const char* wcfn; // WiFi config file name
    static const char* dcfn; // Day config file name
};


#endif //UNTITLED_CONFIGMANAGER_H
