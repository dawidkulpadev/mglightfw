/**
    MioGiapicco Light Firmware - Firmware for Light Device of MioGiapicco system
    Copyright (C) 2026  Dawid Kulpa

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

#ifndef MGLIGHTFW_G2_CONNECTIVITY_H
#define MGLIGHTFW_G2_CONNECTIVITY_H

#include <Arduino.h>
#include "../bleln/BLELNClient.h"
#include "../bleln/BLELNServer.h"
#include "config.h"
#include "SuperString.h"
#include "DeviceConfig.h"
#include "WiFiManager.h"
#include <HTTPUpdate.h>

#define RECENTLY_HAS_BEEN_SERVER_PREFS_TAG  "rhbs"

class ConnectivityServer;
class ConnectivityClient;
class ConnectivityConfig;

class Connectivity {
public:
    enum class ConnectivityMode {ClientMode, ServerMode, ConfigMode};
    typedef std::function<void(int, int, int, const std::string &)> OnApiResponseCb;
    typedef std::function<void(ConnectivityMode)> RequestModeChangeCb;

    void start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences,
               const OnApiResponseCb &onApiResponse);
    void loop();
    void startAPITalk(const std::string& apiPoint, char method, const std::string& data); // Talk with API about me

private:
    Preferences *prefs;

    BLELNServer blelnServer;
    ConnectivityServer *conServer= nullptr;
    ConnectivityClient *conClient= nullptr;
    ConnectivityConfig *conConfig= nullptr;

    WiFiManager wiFiManager;

    // State
    ConnectivityMode conMode= ConnectivityMode::ClientMode;
};



#endif //MGLIGHTFW_G2_CONNECTIVITY_H
