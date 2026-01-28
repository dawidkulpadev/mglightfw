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

#ifndef MGLIGHTFW_WIFIMANAGER_H
#define MGLIGHTFW_WIFIMANAGER_H

#include <WiFi.h>
#include <WebServer.h>

#define WIFI_CONNECT_MAX_DURATION_MS        (15*1000ul)           // 15s
#define WIFI_NTP_MAX_RETIRES                1

class WiFiManager {
public:
    enum class WiFiState {Init, Connecting, NTPSyncing, NTPSyncFailed, Ready, ConnectFailed};
    void startConnect(const std::string &timezone, const std::string &wifiSSID, const std::string &wifiPsk);
    void stop();

    bool isConnected();
    bool isRunning();
    bool hasFailed();

private:
    WiFiState state= WiFiState::Init;
    uint32_t overallFails=0;
    uint8_t ntpRetriesCnt=0;

    bool runMainLoop=false;
    bool loopRunning=false;

    uint32_t timeSyncStartMs;
    uint32_t connectStartMs;

    std::string tz;
    std::string ssid;
    std::string psk;

    void loop();
};


#endif //MGLIGHTFW_WIFIMANAGER_H
