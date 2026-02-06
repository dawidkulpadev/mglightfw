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

#include "ConnectivityClient.h"

#include <utility>

ConnectivityClient::ConnectivityClient(DeviceConfig *deviceConfig, WiFiManager *wifiManager,
                                       Connectivity::OnApiResponseCb onApiResponse,
                                       Connectivity::RequestModeChangeCb requestModeChange) {
    config= deviceConfig;
    oar= std::move(onApiResponse);
    rmc= std::move(requestModeChange);
    state= State::Init;
    connectedFor= ConnectedFor::None;
    meApiTalkRequested= false;
    wm= wifiManager;
    meApiTalkMutex= xSemaphoreCreateMutex();
}


void ConnectivityClient::loop() {
    if(state == State::Init){
        Serial.println("Client mode - Init");
        blelnClient.start(BLE_NAME, [this](const std::string& msg){
            this->onServerResponse(msg);
        });
        lastTimeSync = (millis() - CLIENT_TIME_SYNC_INTERVAL) + 8000; // First time sync in 4 seconds
        firstServerCheckMade= false;
        uint32_t r= (esp_random() / (UINT32_MAX/5))+1;
        Serial.printf("Client mode - First server check in %d seconds\r\n", r*1);
        lastServerCheck = (millis() - CLIENT_SERVER_CHECK_INTERVAL) + 1000ul*r; // Instant server check + x seconds random
        state = State::Idle;
    } else if(state == State::Idle){
        if(firstServerCheckMade and meApiTalkRequested){
            if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(5))==pdTRUE) {
                Serial.println("Client mode - Start API Talk");
                state = State::ServerSearching;
                blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID,
                                              [this](const NimBLEAdvertisedDevice *dev) {
                                                  this->onServerSearchResult(dev);
                                              });
                connectedFor= ConnectedFor::APITalk;
                meApiTalkRequested = false;
                xSemaphoreGive(meApiTalkMutex);
            }
        } else if(firstServerCheckMade and ((millis() - lastTimeSync) >= (CLIENT_TIME_SYNC_INTERVAL)) ){
            Serial.println("Client mode - Start time sync");
            state = State::ServerSearching;
            blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID,
                                          [this](const NimBLEAdvertisedDevice *dev) {
                                              this->onServerSearchResult(dev);
                                          });
            connectedFor= ConnectedFor::TimeSync;
            lastTimeSync = (millis() - CLIENT_TIME_SYNC_INTERVAL) + 8000ul;
        } else if((millis() - lastServerCheck) >= CLIENT_SERVER_CHECK_INTERVAL){
            Serial.println("Client mode - Start server check");
            state= State::ServerChecking;
            blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID,
                                          [this](const NimBLEAdvertisedDevice* dev){
                                              this->onServerSearchResult(dev);
                                          });
            firstServerCheckMade= true;
            lastServerCheck= millis();
        }
    } else if(state == State::ServerConnecting) {

    } else if(state == State::ServerConnected){
        if(connectedFor == ConnectedFor::APITalk){
            char buf[128];
            if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(50))==pdTRUE){
                sprintf(buf, "$ATRQ,1,%s,%c,%s,%s,%s",meApiTalkPoint.c_str(),meApiTalkMethod,meApiTalkMAC.c_str(),meApiTalkPicklock.c_str(),meApiTalkData.c_str());
                xSemaphoreGive(meApiTalkMutex);

                blelnClient.sendEncrypted(buf);
                state=State::WaitingForHTTPResponse;
            }
        } else if(connectedFor == ConnectedFor::TimeSync){
            blelnClient.sendEncrypted("$NTP");
            state=State::WaitingForHTTPResponse;
        }

        // TODO: Add max semaphore take tries
    } else if(state == State::WaitingForHTTPResponse){
        // TODO: Add timeout
    } else if(state == State::HTTPResponseReceived){
        blelnClient.disconnect();
        state= State::Idle;
    } else if(state == State::ServerConnectFailed){
        // TODO: Handle BLELN server connect failed - possibly server had max clients
    } else if(state == State::ServerNotFound){
        // Start WiFi check
        Serial.println("Client mode - No server, checking WiFi...");
        state=State::WiFiChecking;
        wm->startConnect(config->getTimezone(), config->getSsid(), config->getPsk());
    } else if(state == State::WiFiChecking){
        if(wm->isConnected()){
            state= State::WiFiConnected;
        } else if(wm->hasFailed()){
            state= State::WiFiConnectFailed;
        }
    } else if(state == State::WiFiConnected){
        Serial.println("[D] Client mode - WiFi connected. Switching to server");
        switchToServer();
    } else if(state == State::WiFiConnectFailed){
        state= State::Idle;
    }
}


void ConnectivityClient::onServerResponse(const std::string &msg) {
    StringList parts= splitCsvRespectingQuotes(msg);
    if(parts[0]=="$ATRS" and parts.size()==5){
        int rid= strtol(parts[1].c_str(), nullptr, 10);
        int errc= strtol(parts[2].c_str(), nullptr, 10);
        int httpCode= strtol(parts[3].c_str(), nullptr, 10);
        if(rid!=0){
            if(oar)
                oar(rid, errc, httpCode, parts[4]);
        }

        if(state == State::WaitingForHTTPResponse){
            state= State::HTTPResponseReceived;
        }
    } else if(parts[0]=="$HDSH" and parts.size()==2 and parts[1]=="OK"){
        if(state == State::ServerConnecting)
            state= State::ServerConnected;
        else{
            // TODO: Why HDSH received?? Error?
        }
    } else if(parts[0]=="$NTP" and parts.size()==2){
        long nows= strtol(parts[1].c_str(), nullptr, 10);
        setenv("TZ", config->getTimezone(), 1);
        tzset();

        struct timeval tv{};
        tv.tv_sec = nows;
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);

        struct tm timeinfo{};
        getLocalTime(&timeinfo);
        Serial.print(F("Client mode: Time synced - "));
        Serial.println(asctime(&timeinfo));

        if(state == State::WaitingForHTTPResponse){
            state= State::HTTPResponseReceived;
        }

        lastTimeSync= millis();
    }
}


void ConnectivityClient::onServerSearchResult(const NimBLEAdvertisedDevice* dev) {
    Serial.println("Client mode - onServerSearchResult");
    if (dev!= nullptr) {
        if(state == State::ServerSearching) {
            Serial.println("Client mode - BLELN server found. Connecting...");
            blelnClient.beginConnect(dev, [](bool success, int errc) {
                if (!success) {
                    Serial.print("BLELN server connect error: ");
                    Serial.println(errc);
                    if (errc == BLE_REASON_MAX_CLIENTS) {
                        // TODO: If failed due to max clients connected - retry
                    }
                    Serial.print("Failed connecting, reason: ");
                    Serial.println(errc);
                }
            });
            state = State::ServerConnecting;
        } else if(state == State::ServerChecking){
            Serial.println("Client mode - BLELN server found. Continuing as client");
            state= State::Idle;
        }
    } else {
        if(state == State::ServerChecking) {
            Serial.println("Client mode - BLELN server not found");
            state = State::ServerNotFound;
        } else if(state == State::ServerSearching){
            Serial.println("Client mode - BLELN server not found. API talk failed.");
            state = State::Idle;
        }
    }
}

void ConnectivityClient::finish() {
    if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(100))==pdTRUE) {
        meApiTalkRequested= false;
        xSemaphoreGive(meApiTalkMutex);
    }
    blelnClient.stop();
    state= State::Init;
    Serial.println("[D] Client mode - Exited!");
}

void ConnectivityClient::switchToServer() {
    finish();
    rmc(Connectivity::ConnectivityMode::ServerMode);
}


void ConnectivityClient::startAPITalk(const std::string& apiPoint, char method, const std::string &mac, const std::string &picklock, const std::string& data) {
    if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(100))==pdTRUE) {
        meApiTalkPoint = apiPoint;
        meApiTalkMethod = method;
        meApiTalkMAC= mac;
        meApiTalkPicklock= picklock;
        meApiTalkData = data;
        meApiTalkRequested = true;
        xSemaphoreGive(meApiTalkMutex);
    }
}
