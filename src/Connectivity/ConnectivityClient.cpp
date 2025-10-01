//
// Created by dkulpa on 30.09.2025.
//

#include "ConnectivityClient.h"

#include <utility>

ConnectivityClient::ConnectivityClient(DeviceConfig *deviceConfig, Connectivity::OnApiResponseCb onApiResponse,
                                       Connectivity::RequestModeChangeCb requestModeChange) {
    config= deviceConfig;
    oar= std::move(onApiResponse);
    rmc= std::move(requestModeChange);
    state= State::Init;
    connectedFor= ConnectedFor::None;
    wifiState= WiFiStateMachineStates::None;
    meApiTalkRequested= false;
    meApiTalkMutex= xSemaphoreCreateMutex();
    wifi_connectStart= 0;
    wifi_timeSyncStart= 0;
}


void ConnectivityClient::loop() {
    if(state == State::Init){
        Serial.println("Client mode - Init");
        blelnClient.start(BLE_NAME, [this](const std::string& msg){
            this->onServerResponse(msg);
        });
        lastTimeSync = millis() - CLIENT_TIME_SYNC_INTERVAL + 4000; // First time sync in 4 seconds
        state = State::Idle;
    } else if(state == State::Idle){
        if(meApiTalkRequested){
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
        } else if((millis() - lastServerCheck) >= CLIENT_SERVER_CHECK_INTERVAL){
            Serial.println("Client mode - Start server check");
            state= State::ServerChecking;
            blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID,
                                          [this](const NimBLEAdvertisedDevice* dev){
                                              this->onServerSearchResult(dev);
                                          });
            lastServerCheck = millis();
        } else if((millis() - lastTimeSync) >= CLIENT_TIME_SYNC_INTERVAL){
            Serial.println("Client mode - Start time sync");
            state = State::ServerSearching;
            blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID,
                                          [this](const NimBLEAdvertisedDevice *dev) {
                                              this->onServerSearchResult(dev);
                                          });
            connectedFor= ConnectedFor::TimeSync;
            lastTimeSync= millis();
        }
    } else if(state == State::ServerConnecting) {
        if(blelnClient.isConnected()) {
            if (!blelnClient.hasDiscoveredClient()) {
                if (!blelnClient.discover()) {
                    Serial.println("discover fail");
                    state = State::ServerConnectFailed;
                } else {
                    if (!blelnClient.handshake()) {
                        Serial.println("handshake fail");
                        state = State::ServerConnectFailed;
                    } else {
                        Serial.println("handshake OK");
                        // cmState set to ServerConnected in message received callback when HDSH received
                    }
                }
            }
        }
    } else if(state == State::ServerConnected){
        if(connectedFor == ConnectedFor::APITalk){
            char buf[128];
            if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(50))==pdTRUE){
                sprintf(buf, "$ATRQ,1,%s,%c,%s",meApiTalkPoint.c_str(),meApiTalkMethod,meApiTalkData.c_str());
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

        // Release
        // WiFi.begin(config->getSsid(), config->getPsk());
        // Debug
        WiFiClass::mode(WIFI_STA);
        WiFi.begin("S101A_B", "gumowy_kot_wiadro_obiera");
        wifi_connectStart= millis();
        ntpRetriesCnt=0;
        wifiState = WiFiStateMachineStates::Connecting;
    } else if(state == State::WiFiChecking){
        if(wifiState == WiFiStateMachineStates::Connecting){
            if(WiFi.isConnected()){
                syncWithNTP(config->getTimezone());
                Serial.println("Connectivity (Normal): Waiting for NTP time sync...");
                wifiState= WiFiStateMachineStates::NTPSyncing;
            } else if((millis()-wifi_connectStart) >= WIFI_CONNECT_MAX_DURATION_MS){
                Serial.println("Connectivity (Client): WiFi connectiong timeout");
                wifiState= WiFiStateMachineStates::ConnectFailed;
            }
        } else if(wifiState == WiFiStateMachineStates::NTPSyncing){ // Wait for time sync with NTP
            time_t nowSecs = time(nullptr);
            if(nowSecs < (60*60*24*365*30)){ // 60s * 60m * 24h * 365days * 30years
                if((millis() - wifi_timeSyncStart) >= (15 * 1000)){ // Wait max 15s
                    Serial.println("Connectivity (Normal): Time sync failed! (inf loop)");
                    wifiState= WiFiStateMachineStates::NTPSyncFailed;
                }
            } else {
                struct tm timeinfo{};
                getLocalTime(&timeinfo);
                Serial.print(F("Connectivity (Normal): Time synced - "));
                Serial.println(asctime(&timeinfo));

                wifiState= WiFiStateMachineStates::Ready;
                state= State::WiFiConnected;
            }
        } else if(wifiState == WiFiStateMachineStates::NTPSyncFailed){
            if(ntpRetriesCnt<WIFI_NTP_MAX_RETIRES){
                Serial.println("Connectivity (Normal): Time sync will be retried!");
                wifiState= WiFiStateMachineStates::Connecting;
                wifi_connectStart= millis();
                ntpRetriesCnt++;
            } else {
                state= State::WiFiConnectFailed;
            }
        } else if(wifiState == WiFiStateMachineStates::ConnectFailed){
            Serial.println("Connectivity (Normal): WiFi connect failed!");
            WiFi.scanDelete();
            WiFi.persistent(false);
            WiFi.disconnect(false, false);
            wifi_overallFails++;
            state= State::WiFiConnectFailed;
        }
    } else if(state == State::WiFiConnected){
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
        Serial.println("Client mode - BLELN server not found");
        state = State::ServerNotFound;
    }
}

void ConnectivityClient::finish() {
    if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(100))==pdTRUE) {
        meApiTalkRequested= false;
        xSemaphoreGive(meApiTalkMutex);
    }
    blelnClient.stop();
    state= State::Init;
}

void ConnectivityClient::switchToServer() {
    finish();
    rmc(Connectivity::ConnectivityMode::ServerMode);
}

bool ConnectivityClient::syncWithNTP(const std::string &tz) {
    configTzTime(tz.c_str(), "pool.ntp.org");
    wifi_timeSyncStart= millis();

    return true;
}

void ConnectivityClient::startAPITalk(const std::string& apiPoint, char method, const std::string& data) {
    if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(100))==pdTRUE) {
        meApiTalkPoint = apiPoint;
        meApiTalkMethod = method;
        meApiTalkData = data;
        meApiTalkRequested = true;
        xSemaphoreGive(meApiTalkMutex);
    }
}
