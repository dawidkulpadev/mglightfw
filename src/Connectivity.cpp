//
// Created by dkulpa on 20.08.2025.
//

#include <utility>
#include <vector>
#include "Connectivity.h"
#include "esp_heap_caps.h"

void Connectivity::start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences,
                         std::function<void(int, int, int, const std::string &msg)> onApiResponse) {
    prefs= preferences;
    config= devConfig;
    if(devMode==DEVICE_MODE_CONFIG)
        conMode= ConnectivityMode::ConfigMode;
    else
        conMode= ConnectivityMode::ClientMode;
    cfmState= ConfigModeState::Start;
    smState= ServerModeState::Init;
    cmState= ClientModeState::Init;
    wifiState = WiFiStateMachineStates::None;
    WiFi.macAddress(mac);
    meApiTalkRequested= false;
    meApiTalkMutex= xSemaphoreCreateMutex();
    oar= std::move(onApiResponse);
}

void Connectivity::loop() {
    switch(conMode){
        case ConnectivityMode::ClientMode:
            cli_loop();
            break;
        case ConnectivityMode::ServerMode:
            ser_loop();
            break;
        case ConnectivityMode::ConfigMode:
            conf_loop();
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Connectivity::startAPITalk(const std::string& apiPoint, char method, const std::string& data) {
    if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(100)==pdTRUE)) {
        meApiTalkPoint = apiPoint;
        meApiTalkMethod = method;
        meApiTalkData = data;
        meApiTalkRequested = true;
        xSemaphoreGive(meApiTalkMutex);
    }
}

void Connectivity::conf_loop() {
    if(cfmState==ConfigModeState::Start){
        Serial.println("Connectivity (Config): Start");

        blelnServer.start(prefs, BLE_NAME, BLELN_CONFIG_UUID);
        blelnServer.setOnMessageReceivedCallback([this](uint16_t cliH, const std::string &msg){
            StringList parts= splitCsvRespectingQuotes(msg);
            if(parts[0]=="$CONFIG"){
                char resp[256];

                if(parts[1]=="GET"){
                    if(parts[2]=="wssid"){
                        if(this->config->getSsid()!= nullptr)
                            sprintf(resp, "$CONFIG,VAL,wssid,%s",this->config->getSsid());
                        else
                            sprintf(resp, "$CONFIG,VAL,wssid,");
                    } else if(parts[2]=="pcklk"){
                        if(this->config->getPicklock()!= nullptr)
                            sprintf(resp, "$CONFIG,VAL,pcklk,%s",this->config->getPicklock());
                        else
                            sprintf(resp, "$CONFIG,VAL,pcklk,");
                    } else if(parts[2]=="tzone"){
                        if(this->config->getTimezone()!= nullptr)
                            sprintf(resp, "$CONFIG,VAL,tzone,%s",this->config->getTimezone());
                        else
                            sprintf(resp, "$CONFIG,VAL,tzone,");
                    } else if(parts[2]=="mac"){
                        char str_mac[14];
                        uint8_t *macc= this->getMAC();
                        sprintf(str_mac, "%02X%02X%02X%02X%02X%02X", macc[0], macc[1], macc[2], macc[3], macc[4], macc[5]);
                        sprintf(resp, "$CONFIG,VAL,mac,%s", str_mac);
                    }
                } else if(parts[1]=="SET"){
                    if(parts[2]=="wssid"){
                        sprintf(resp,"$CONFIG,SETOK,wssid");
                        this->config->setSsid(parts[3].c_str());
                    } else if(parts[2]=="wpsk"){
                        sprintf(resp,"$CONFIG,SETOK,wpsk");
                        this->config->setPsk(parts[3].c_str());
                    } else if(parts[2]=="pcklk"){
                        sprintf(resp,"$CONFIG,SETOK,pcklk");
                        this->config->setPicklock(parts[3].c_str());
                    } else if(parts[2]=="tzone"){
                        sprintf(resp,"$CONFIG,SETOK,tzone");
                        this->config->setTimezone(parts[3].c_str());
                    } else if(parts[2]=="uid"){
                        sprintf(resp,"$CONFIG,SETOK,uid");
                        this->config->setUid(parts[3].c_str());
                    }
                }

                if(strlen(resp)>0){
                    blelnServer.sendEncrypted(cliH, resp);
                }
            } else if(parts[0]=="$REBOOT"){
                conf_rebootCalledAt= millis();
                conf_rebootCalled= true;
            }
        });

        WiFiClass::mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.scanNetworks(true, false, false, 300);

        cfmState = ConfigModeState::ServerTasking;
    } else if(cfmState==ConfigModeState::ServerTasking){
        std::string resStr;
        int16_t r= WiFi.scanComplete();

        if(r>=0) {
            resStr="";
            for(uint16_t i=0; i<r; i++){
                resStr+= WiFi.SSID(i).c_str();
                resStr+= "@";
                resStr+= std::to_string(WiFi.RSSI(i));

                if(i!=(r-1))
                    resStr += ";";
            }

            WiFi.scanNetworks(true, false, false, 300);
            blelnServer.sendEncrypted(("$WIFIL,"+resStr));
        }

        if(conf_rebootCalled){
            if(conf_rebootCalledAt + 2000 < millis()){
                ConfigManager::writeWifi(prefs, this->config);
                esp_restart();
            }
        }
    }
}

/************************ Server mode methods *******************************/

void Connectivity::ser_loop() {
    if(smState==ServerModeState::Init){
        Serial.println("Server mode - Init");
        runAPITalksWorker= true;
        xTaskCreatePinnedToCore(
                [](void* arg){
                    static_cast<Connectivity*>(arg)->apiTalksWorker();
                    vTaskDelete(nullptr);
                },
                "ATWrkr", 4096, this, 5, nullptr, 1);

        blelnServer.start(prefs, BLE_NAME, BLELN_HTTP_REQUESTER_UUID);
        blelnServer.setOnMessageReceivedCallback([this](uint16_t cliH, const std::string &msg){
            this->ser_onMessageReceived(cliH, msg);
        });
        smState= ServerModeState::Idle;
    } else if(smState==ServerModeState::Idle){
        APITalkResponse pkt{};
        if (xQueueReceive(apiTalksResponseQueue, &pkt, 0) == pdTRUE) {
            if(pkt.h!=UINT16_MAX) {
                Serial.println("Sending response");
                char msgBuf[220];
                sprintf(msgBuf, "$ATRS,%d,%d,%d,\"%s\"", pkt.id, pkt.errc, pkt.respCode, pkt.data);

                bool r = blelnServer.sendEncrypted(pkt.h, msgBuf);
                Serial.print("Send result: ");
                Serial.println(std::to_string(r).c_str());
            } else {
                if(oar)
                    oar(pkt.id,pkt.errc,pkt.respCode,pkt.data);
            }
            free(pkt.data);
        }

        // Forward my own API Talk if requested
        if(xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(5)==pdTRUE)) {
            if (meApiTalkRequested) {
                Serial.println("Server mode - API Talk requested");
                appendToAPITalksRequestQueue(UINT16_MAX, UINT16_MAX, meApiTalkPoint, meApiTalkMethod, meApiTalkData);
                meApiTalkRequested = false;
            }
            xSemaphoreGive(meApiTalkMutex);
        }

        if(blelnServer.noClientsConnected() and ser_lastServerSearch + BLELN_SERVER_SEARCH_INTERVAL_MS < millis()){
            ser_lastServerSearch= millis();
            blelnServer.startOtherServerSearch(5000, BLELN_HTTP_REQUESTER_UUID, [this](bool found){
                if(found){
                    Serial.println("Server mode - Switching to client mode (cleanup)...");
                    this->smState=ServerModeState::OtherBLELNServerFound;
                }

                this->ser_lastServerSearch= millis();
            });
        }
    } else if(smState==ServerModeState::OtherBLELNServerFound){
        APITalkResponse pkt{};
        if (xQueueReceive(apiTalksResponseQueue, &pkt, 0) == pdTRUE) {
            if(pkt.h!=UINT16_MAX) {
                Serial.println("Sending response");
                char msgBuf[220];
                sprintf(msgBuf, "$ATRS,%d,%d,%d,\"%s\"", pkt.id, pkt.errc, pkt.respCode, pkt.data);

                bool r = blelnServer.sendEncrypted(pkt.h, msgBuf);
                Serial.print("Send result: ");
                Serial.println(std::to_string(r).c_str());
            } else {
                if(oar)
                    oar(pkt.id,pkt.errc,pkt.respCode,pkt.data);
            }
            free(pkt.data);
        }

        if(uxQueueMessagesWaiting(apiTalksResponseQueue)==0 and uxQueueMessagesWaiting(apiTalksRequestQueue))
            ser_switchToClient();
    }
}

void Connectivity::ser_finish() {
    runAPITalksWorker= false;
    blelnServer.stop();
}

void Connectivity::ser_switchToClient() {
    Serial.println("Server mode - switch to client mode");
    ser_finish();
    smState= ServerModeState::Init;
    conMode= ConnectivityMode::ClientMode;
}


void Connectivity::ser_onMessageReceived(uint16_t cliH, const std::string &msg) {
    StringList parts= splitCsvRespectingQuotes(msg);
    /**
     * API talk request
     * $ATRQ,id,api_point,method,data
     *  * id - request id, passed to response, client defined, not zero!
     *  * api_point - api function url without server address eg. light/get.php
     *  * method - P for POST, G for GET
     *  * data - data string attached to API request
     */
    if(parts[0]=="$ATRQ"){
        uint16_t id= strtol(parts[1].c_str(), nullptr, 10);
        if(id!=0) {
            char method = parts[3].c_str()[0];

            if (method == 'P' or method == 'G')
                appendToAPITalksRequestQueue(cliH, id, parts[2], parts[3].c_str()[0], parts[4]);
        }
    }
}


/************************ Server mode methods END *******************************/



/************************ Client mode methods *******************************/
void Connectivity::cli_loop() {
    if(cmState==ClientModeState::Init){
        Serial.println("Client mode - Init");
        blelnClient.start(BLE_NAME, [this](const std::string& msg){
            this->cli_onServerResponse(msg);
        });
        cmState = ClientModeState::Idle;
    } else if(cmState==ClientModeState::Idle){
        if((xSemaphoreTake(meApiTalkMutex, pdMS_TO_TICKS(5)==pdTRUE)) and meApiTalkRequested){
            Serial.println("Client mode - Start API Talk");
            cmState = ClientModeState::ServerSearching;
            blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID,
                                          [this](const NimBLEAdvertisedDevice *dev) {
                                              this->cli_onServerSearchResult(dev);
                                          });
            meApiTalkRequested= false;
            xSemaphoreGive(meApiTalkMutex);
        } else if(cli_lastServerCheck + CLIENT_SERVER_CHECK_INTERVAL < millis()){
            Serial.println("Client mode - Start server check");
            cmState= ClientModeState::ServerChecking;
            blelnClient.startServerSearch(5000, BLELN_HTTP_REQUESTER_UUID, [this](const NimBLEAdvertisedDevice* dev){
                this->cli_onServerSearchResult(dev);
            });
            cli_lastServerCheck = millis();
        }
    } else if(cmState==ClientModeState::ServerConnecting) {
        if(blelnClient.isConnected()) {
            if (!blelnClient.hasDiscoveredClient()) {
                if (!blelnClient.discover()) {
                    Serial.println("discover fail");
                    cmState = ClientModeState::ServerConnectFailed;
                } else {
                    if (!blelnClient.handshake()) {
                        Serial.println("handshake fail");
                        cmState = ClientModeState::ServerConnectFailed;
                    } else {
                        Serial.println("handshake OK");
                    }
                }
            }
        }
    } else if(cmState==ClientModeState::ServerConnected){
        // TODO: Send API request to server
        //blelnClient.sendEncrypted();
        cmState=ClientModeState::WaitingForHTTPResponse;
    } else if(cmState==ClientModeState::WaitingForHTTPResponse){
        blelnClient.disconnect();
        cmState= ClientModeState::Idle;
        // TODO: If response received, disconnect and go to ClientIdle
        // TODO: Add timeout
    } else if(cmState==ClientModeState::ServerConnectFailed){
        // TODO: Handle BLELN server connect failed - possibly server had max clients
    } else if(cmState==ClientModeState::ServerNotFound){
        // Start WiFi check
        Serial.println("Client mode - No server, checking WiFi...");
        cmState=ClientModeState::WiFiChecking;

        // Release
        // WiFi.begin(config->getSsid(), config->getPsk());
        // Debug
        WiFiClass::mode(WIFI_STA);
        wl_status_t ret = WiFi.begin("dlink3", "sikakama2");
        Serial.printf("[APP] WiFi.begin() returned %d\n", ret);
        wifi_connectStart= millis();
        ntpRetriesCnt=0;
        wifiState = WiFiStateMachineStates::Connecting;
    } else if(cmState==ClientModeState::WiFiChecking){
        if(wifiState == WiFiStateMachineStates::Connecting){
            if(WiFi.isConnected()){
                syncWithNTP(config->getTimezone());
                Serial.println("Connectivity (Normal): Waiting for NTP time sync...");
                wifiState= WiFiStateMachineStates::NTPSyncing;
            } else if(wifi_connectStart + WIFI_CONNECT_MAX_DURATION_MS < millis()){
                Serial.println("Connectivity (Client): WiFi connectiong timeout");
                wifiState= WiFiStateMachineStates::ConnectFailed;
            }
        } else if(wifiState == WiFiStateMachineStates::NTPSyncing){ // Wait for time sync with NTP
            time_t nowSecs = time(nullptr);
            if(nowSecs < (60*60*24*365*30)){ // 60s * 60m * 24h * 365days * 30years
                if(wifi_timeSyncStart + (15 * 1000) < millis()){ // Wait max 15s
                    Serial.println("Connectivity (Normal): Time sync failed! (inf loop)");
                    wifiState= WiFiStateMachineStates::NTPSyncFailed;
                }
            } else {
                struct tm timeinfo{};
                getLocalTime(&timeinfo);
                Serial.print(F("Connectivity (Normal): Time synced - "));
                Serial.println(asctime(&timeinfo));

                wifiState= WiFiStateMachineStates::Ready;
                cmState= ClientModeState::WiFiConnected;
            }
        } else if(wifiState == WiFiStateMachineStates::NTPSyncFailed){
            if(ntpRetriesCnt<WIFI_NTP_MAX_RETIRES){
                Serial.println("Connectivity (Normal): Time sync will be retried!");
                wifiState= WiFiStateMachineStates::Connecting;
                wifi_connectStart= millis();
                ntpRetriesCnt++;
            } else {
                cmState= ClientModeState::WiFiConnectFailed;
            }
        } else if(wifiState == WiFiStateMachineStates::ConnectFailed){
            Serial.println("Connectivity (Normal): WiFi connect failed!");
            WiFi.scanDelete();
            WiFi.persistent(false);
            WiFi.disconnect(false, false);
            wifi_overallFails++;
            cmState= ClientModeState::WiFiConnectFailed;
        }
    } else if(cmState==ClientModeState::WiFiConnected){
        cli_switchToServer();
    } else if(cmState==ClientModeState::WiFiConnectFailed){
        cmState= ClientModeState::Idle;
    }
}

void Connectivity::cli_onServerResponse(const std::string &msg) {
    StringList parts= splitCsvRespectingQuotes(msg);
    if(parts[0]=="$ATRS" and parts.size()==5){
        int rid= strtol(parts[1].c_str(), nullptr, 10);
        int errc= strtol(parts[2].c_str(), nullptr, 10);
        int httpCode= strtol(parts[3].c_str(), nullptr, 10);
        if(rid!=0){
            if(oar)
                oar(rid, errc, httpCode, parts[4]);
        }
    } else if(parts[0]=="$HDSH" and parts.size()==2 and parts[1]=="OK"){
        if(cmState==ClientModeState::ServerConnecting)
            cmState= ClientModeState::ServerConnected;
        else{
            // TODO: Why HDSH received?? Error?
        }
    }
}


void Connectivity::cli_onServerSearchResult(const NimBLEAdvertisedDevice* dev) {
    if (dev!= nullptr) {
        if(cmState==ClientModeState::ServerSearching) {
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
            cmState = ClientModeState::ServerConnecting;
        } else if(cmState==ClientModeState::ServerChecking){
            Serial.println("Client mode - BLELN server found. Continuing as client");
            cmState= ClientModeState::Idle;
        }
    } else {
        Serial.println("Client mode - BLELN server not found");
        cmState = ClientModeState::ServerNotFound;
    }
}

void Connectivity::cli_finish() {
    blelnClient.stop();
    apiTalksRequestQueue= xQueueCreate(20, sizeof(APITalkRequest));
    apiTalksResponseQueue= xQueueCreate(20, sizeof(APITalkResponse));
    cmState= ClientModeState::Init;
}

void Connectivity::cli_switchToServer() {
    cli_finish();
    conMode= ConnectivityMode::ServerMode;
}
/************************ Client mode methods END *******************************/


bool Connectivity::syncWithNTP(const std::string &tz) {
    configTzTime(tz.c_str(), "pool.ntp.org");
    wifi_timeSyncStart= millis();

    return true;
}

uint8_t *Connectivity::getMAC() {
    return mac;
}

void Connectivity::appendToAPITalksResponseQueue(uint16_t h, uint16_t id, uint8_t errc, uint16_t respCode, const String& data) {
    auto* dataHeapBuf= (char*)malloc(data.length()+1);
    if (!dataHeapBuf) return;

    strcpy(dataHeapBuf, data.c_str());

    APITalkResponse pkt{h, id, errc, respCode, dataHeapBuf};
    if (xQueueSend(apiTalksResponseQueue, &pkt, 0) != pdPASS) {
        free(dataHeapBuf);
    } else {
        Serial.println("Pushed response");
    }
}

void
Connectivity::appendToAPITalksRequestQueue(uint16_t h, uint16_t id, const std::string &apiPoint, char method, const std::string &data) {
    auto* apiPointHeapBuf = (char*)malloc(apiPoint.size()+1);
    auto* dataHeapBuf= (char*)malloc(data.size()+1);
    if (!apiPointHeapBuf and !dataHeapBuf) return;
    strcpy(apiPointHeapBuf, apiPoint.c_str());
    strcpy(dataHeapBuf, data.c_str());

    APITalkRequest pkt{h, id, method, apiPointHeapBuf, dataHeapBuf};
    if (xQueueSend(apiTalksRequestQueue, &pkt, 0) != pdPASS) {
        free(apiPointHeapBuf);
        free(dataHeapBuf);
    }
}

void Connectivity::apiTalksWorker() {
    // TODO: Handle updates

    while(runAPITalksWorker){
        APITalkRequest pkt{};
        if (xQueueReceive(apiTalksRequestQueue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE) {
            Serial.println("New request received");
            std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
            client->setCACertBundle(rootca_crt_bundle_start);

            HTTPClient https;

            // Create request
            unsigned int httpReqLen = api_url.size() + strlen(pkt.apiPoint);
            if (pkt.method == 'G')
                httpReqLen += strlen(pkt.data);

            std::string httpReq;
            httpReq.reserve(httpReqLen + 20);
            httpReq.append(api_url).append(pkt.apiPoint);
            if (pkt.method == 'G') {
                httpReq.push_back('?');
                httpReq.append(pkt.data);
            }

            Serial.println(httpReq.c_str());
            Serial.println(pkt.data);

            if (https.begin(*client, httpReq.c_str())) {  // HTTPS
                int httpCode = 0;
                if (pkt.method == 'P') {
                    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
                    httpCode = https.POST(pkt.data);
                } else if (pkt.method == 'G') {
                    httpCode = https.GET();
                }

                // httpCode will be negative on error
                if (httpCode > 0) {
                    // HTTP header has been send and Server response header has been handled
                    appendToAPITalksResponseQueue(pkt.h, pkt.id, 0, httpCode, https.getString());
                } else {
                    appendToAPITalksResponseQueue(pkt.h, pkt.id, 3, 0, "");
                    Serial.printf("[HTTPS] POST... failed, error: %s\n", HTTPClient::errorToString(httpCode).c_str());
                }

                https.end();
            } else {
                appendToAPITalksResponseQueue(pkt.h, pkt.id, 2, 0, "");
                Serial.println("Error https begin");
            }

            free(pkt.apiPoint);
            free(pkt.data);
        }

        if(uxQueueMessagesWaiting(apiTalksRequestQueue)>0){
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if(lastWaterMarkPrint+10000 < millis()) {
            UBaseType_t freeWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("Connectivity API talks stack free: %u\n\r",
                          freeWords);
            lastWaterMarkPrint= millis();
        }
    }
}












