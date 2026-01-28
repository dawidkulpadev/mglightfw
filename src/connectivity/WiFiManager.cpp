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

#include "WiFiManager.h"

void WiFiManager::startConnect(const std::string &timezone, const std::string &wifiSSID, const std::string &wifiPsk) {
    if(!runMainLoop and !loopRunning){
        tz= timezone;
        ssid= wifiSSID;
        psk= wifiPsk;
        runMainLoop=true;
        ntpRetriesCnt= 0;
        state= WiFiState::Init;
        xTaskCreatePinnedToCore([](void* arg){
            static_cast<WiFiManager*>(arg)->loop();
            vTaskDelete(nullptr);
        }, "WiFiMgr", 2500, this, 5, nullptr, 1);
    }
}

void WiFiManager::stop() {
    runMainLoop= false;
}

void WiFiManager::loop() {
    connectStartMs= millis();
    WiFiClass::mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), psk.c_str());
    state= WiFiState::Connecting;

    while (runMainLoop) {
        if (state == WiFiState::Connecting) {
            if (WiFi.isConnected()) {
                configTzTime(tz.c_str(), "pool.ntp.org");
                timeSyncStartMs= millis();
                Serial.println("WiFi Manager - Waiting for NTP time sync...");
                state = WiFiState::NTPSyncing;
            } else if ((millis() - connectStartMs) >= WIFI_CONNECT_MAX_DURATION_MS) {
                Serial.println("WiFi Manager - WiFi connectiong timeout");
                state = WiFiState::ConnectFailed;
            }
        } else if (state == WiFiState::NTPSyncing) { // Wait for time sync with NTP
            time_t nowSecs = time(nullptr);
            if (nowSecs < (60 * 60 * 24 * 365 * 30)) { // 60s * 60m * 24h * 365days * 30years
                if ((millis() - timeSyncStartMs) >= (15 * 1000)) { // Wait max 15s
                    Serial.println("WiFi Manager - Time sync failed! (inf loop)");
                    state = WiFiState::NTPSyncFailed;
                }
            } else {
                struct tm timeinfo{};
                getLocalTime(&timeinfo);
                Serial.print(F("WiFi Manager - Time synced - "));
                Serial.println(asctime(&timeinfo));

                state = WiFiState::Ready;
            }
        } else if (state == WiFiState::NTPSyncFailed) {
            if (ntpRetriesCnt < WIFI_NTP_MAX_RETIRES) {
                Serial.println("WiFi Manager - Time sync will be retried!");
                state = WiFiState::Connecting;
                connectStartMs = millis();
                ntpRetriesCnt++;
            } else {
                state = WiFiState::ConnectFailed;
            }
        } else if (state == WiFiState::ConnectFailed) {
            Serial.println("WiFi Manager - WiFi connect failed!");
            runMainLoop= false;
            overallFails++;
        } else if (state == WiFiState::Ready){
            if(!WiFi.isConnected()){
                connectStartMs= millis();
                state= WiFiState::Connecting;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // Cleanup
    WiFi.scanDelete();
    WiFi.persistent(false);
    WiFi.disconnect(false, false);
}

bool WiFiManager::isConnected() {
    return state==WiFiState::Ready;
}

bool WiFiManager::hasFailed() {
    return state==WiFiState::ConnectFailed;
}

bool WiFiManager::isRunning() {
    return loopRunning;
}
