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

#include <Preferences.h>
#include "DeviceConfig.h"
#include "Day.h"

#define CONFIGMANAGER_KEY_PSK       "psk"
#define CONFIGMANAGER_KEY_SSID      "ssid"
#define CONFIGMANAGER_KEY_PICKLOCK  "picklock"
#define CONFIGMANAGER_KEY_UID       "uid"
#define CONFIGMANAGER_KEY_TIMEZONE  "tz"
#define CONFIGMANAGER_KEY_ROLE      "role"

#define CONFIGMANAGER_KEY_DLI       "dli"
#define CONFIGMANAGER_KEY_DS        "ds"
#define CONFIGMANAGER_KEY_DE        "de"
#define CONFIGMANAGER_KEY_SSD       "ssd"
#define CONFIGMANAGER_KEY_SRD       "srd"


class ConfigManager {
public:
    static bool readDeviceConfig(Preferences *prefs, DeviceConfig *config);
    static bool writeDeviceConfig(Preferences *prefs, DeviceConfig *config);
    static bool writeDay(Preferences *prefs, Day *day);
    static bool readDay(Preferences *prefs, Day *day);
    static bool clearWifiConfig(Preferences *prefs);
};


#endif //UNTITLED_CONFIGMANAGER_H
