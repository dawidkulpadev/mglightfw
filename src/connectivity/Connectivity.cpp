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

#include <vector>
#include "Connectivity.h"
#include "esp_heap_caps.h"
#include "ConnectivityServer.h"
#include "ConnectivityClient.h"
#include "ConnectivityConfig.h"

void Connectivity::start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences,
                         const OnApiResponseCb &onApiResponse) {
    prefs= preferences;

    if(devMode==DEVICE_MODE_CONFIG) {
        conConfig= new ConnectivityConfig(&blelnServer, preferences, devConfig);
        conMode = ConnectivityMode::ConfigMode;
    } else {
        conClient= new ConnectivityClient(devConfig, &wiFiManager, onApiResponse,
                                          [this](ConnectivityMode m){
            this->conMode= m;
        });
        conServer= new ConnectivityServer(&blelnServer, devConfig, preferences, &wiFiManager, onApiResponse,
                                          [this](ConnectivityMode m){
            this->conMode= m;
        });

        /* bool rhbs= preferences->getBool(RECENTLY_HAS_BEEN_SERVER_PREFS_TAG, false); // Recently has been in server mode
        if(rhbs) {
            Serial.println("Connectivity - Recently has been server. Start as server");
            conMode = ConnectivityMode::ServerMode;
        } else {
            Serial.println("Connectivity - Recently has been client. Start as client");
            conMode = ConnectivityMode::ClientMode;
        } */
        Serial.println("[I] Connectivity - Start as client");
        conMode = ConnectivityMode::ClientMode;
     }
}

void Connectivity::loop() {
    switch(conMode){
        case ConnectivityMode::ClientMode:
            if(conClient!= nullptr)
                conClient->loop();
            break;
        case ConnectivityMode::ServerMode:
            if(conServer!= nullptr)
                conServer->loop();
            break;
        case ConnectivityMode::ConfigMode:
            if(conConfig!= nullptr)
                conConfig->loop();
            break;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}

void Connectivity::startAPITalk(const std::string& apiPoint, char method, const std::string& data) {
    if(conMode==ConnectivityMode::ClientMode) {
        prefs->putBool(RECENTLY_HAS_BEEN_SERVER_PREFS_TAG, false);
        conClient->startAPITalk(apiPoint, method, data);
    } else if(conMode==ConnectivityMode::ServerMode)
        prefs->putBool(RECENTLY_HAS_BEEN_SERVER_PREFS_TAG, true);
        conServer->requestApiTalk(method, apiPoint, data);
}








