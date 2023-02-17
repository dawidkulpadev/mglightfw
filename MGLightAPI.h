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

#ifndef UNTITLED_MGLIGHTAPI_H
#define UNTITLED_MGLIGHTAPI_H


#include <Arduino.h>
#include <LittleFS.h>

#include <Ticker.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "caCertsBundle.h"

#include "ConfigManager.h"
#include "Day.h"
#include <cstring>

#define MGLIGHTAPI_POST_DATA_FORMAT "id=%02X%02X%02X%02X%02X%02X&uid=%d&picklock=%s"

using namespace std;

class MGLightAPI {
public:
    MGLightAPI(int uid, const char* picklock, Day *day);
    void talkWithServer();

private:
    static int getUIntValue(const String &text, char* key);
    static int getDayTimeValue(const String &text, char* key);
    u_int64_t id;
    int uid;
    char* picklock;
    Day *day;

};


#endif //UNTITLED_MGLIGHTAPI_H
