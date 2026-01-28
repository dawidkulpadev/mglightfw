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

#include <memory>
#include <utility>
#include "ConnectivityServer.h"

ConnectivityServer::ConnectivityServer(BLELNServer *blelnServer, DeviceConfig *deviceConfig, Preferences *preferences,
                                       WiFiManager *wifiManager, Connectivity::OnApiResponseCb onApiResponse,
                                       Connectivity::RequestModeChangeCb requestModeChange) {
    this->blelnServer= blelnServer;
    oar= std::move(onApiResponse);
    rmc= std::move(requestModeChange);
    config= deviceConfig;
    prefs= preferences;
    state= ServerModeState::Init;
    runAPITalksWorker= false;
    apiTalksRequestQueue= nullptr;
    apiTalksResponseQueue= nullptr;
    wm= wifiManager;
}

void ConnectivityServer::loop() {
    if(state==ServerModeState::Init){
        Serial.println("Server mode - Init");
        apiTalksRequestQueue= xQueueCreate(20, sizeof(APITalkRequest));
        apiTalksResponseQueue= xQueueCreate(20, sizeof(APITalkResponse));
        runAPITalksWorker= true;
        xTaskCreatePinnedToCore(
                [](void* arg){
                    static_cast<ConnectivityServer*>(arg)->apiTalksWorker();
                    vTaskDelete(nullptr);
                },
                "ATWrkr", 4096, this, 5, nullptr, 1);

        blelnServer->start(prefs, BLE_NAME, BLELN_HTTP_REQUESTER_UUID);
        blelnServer->setOnMessageReceivedCallback([this](uint16_t cliH, const std::string &msg){
            this->onMessageReceived(cliH, msg);
        });
        state= ServerModeState::Idle;


    } else if(state==ServerModeState::Idle){
        if(wm->isConnected()) {
            handleAPIResponse();

            if (blelnServer->noClientsConnected() and
                ((millis() - lastServerSearch) >= BLELN_SERVER_SEARCH_INTERVAL_MS)) {
                lastServerSearch = millis();
                blelnServer->startOtherServerSearch(5000, BLELN_HTTP_REQUESTER_UUID, [this](bool found) {
                    if (found) {
                        Serial.println("Server mode - Switching to client mode (cleanup)...");
                        this->state = ServerModeState::OtherBLELNServerFound;
                    }

                    this->lastServerSearch = millis();
                });
            }
        } else if (wm->hasFailed()) {
            // TODO: Become client
        } else if(!wm->isRunning()){
            wm->startConnect(config->getTimezone(), "dlink3", "sikakama2");
        }
    } else if(state==ServerModeState::OtherBLELNServerFound){
        handleAPIResponse();

        if(uxQueueMessagesWaiting(apiTalksResponseQueue)==0 and uxQueueMessagesWaiting(apiTalksRequestQueue))
            switchToClient();
    }
}

void ConnectivityServer::finish() {
    runAPITalksWorker= false;
    blelnServer->stop();
}

void ConnectivityServer::switchToClient() {
    Serial.println("Server mode - switch to client mode");
    finish();
    state= ServerModeState::Init;
    rmc(Connectivity::ConnectivityMode::ClientMode);
}


void ConnectivityServer::onMessageReceived(uint16_t cliH, const std::string &msg) {
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
    } else if(parts[0]=="$NTP"){
        auto nows= static_cast<uint32_t>(time(nullptr));
        char buf[22];
        sprintf(buf, "$NTP,%d", nows);
        blelnServer->sendEncrypted(cliH, buf);
    } else if(parts[0]=="$UPD"){
        uint32_t fwId= strtoul(parts[1].c_str(), nullptr, 10);
        uint16_t sector= strtol(parts[2].c_str(), nullptr, 10);
        uint16_t page= strtol(parts[3].c_str(), nullptr, 10);


    }
}


void ConnectivityServer::handleAPIResponse() {
    APITalkResponse pkt{};
    if (xQueueReceive(apiTalksResponseQueue, &pkt, 0) == pdTRUE) {
        if(pkt.h!=UINT16_MAX) {
            Serial.println("Sending response");
            char msgBuf[220];
            sprintf(msgBuf, "$ATRS,%d,%d,%d,\"%s\"", pkt.id, pkt.errc, pkt.respCode, pkt.data);

            bool r = blelnServer->sendEncrypted(pkt.h, msgBuf);
            Serial.print("Send result: ");
            Serial.println(std::to_string(r).c_str());
        } else {
            if(oar)
                oar(pkt.id,pkt.errc,pkt.respCode,pkt.data);
        }
        free(pkt.data);
    }
}

void ConnectivityServer::appendToAPITalksResponseQueue(uint16_t h, uint16_t id, uint8_t errc, uint16_t respCode,
                                                       const String& data) {
    if(apiTalksResponseQueue!= nullptr) {
        auto *dataHeapBuf = (char *) malloc(data.length() + 1);
        if (!dataHeapBuf) return;

        strcpy(dataHeapBuf, data.c_str());

        APITalkResponse pkt{h, id, errc, respCode, dataHeapBuf};
        if (xQueueSend(apiTalksResponseQueue, &pkt, 0) != pdPASS) {
            free(dataHeapBuf);
        } else {
            Serial.println("Pushed response");
        }
    }
}

void ConnectivityServer::appendToAPITalksRequestQueue(uint16_t h, uint16_t id, const std::string &apiPoint,
                                                      char method, const std::string &data) {
    if(apiTalksRequestQueue!= nullptr) {
        auto *apiPointHeapBuf = (char *) malloc(apiPoint.size() + 1);
        auto *dataHeapBuf = (char *) malloc(data.size() + 1);
        if (!apiPointHeapBuf and !dataHeapBuf) return;
        strcpy(apiPointHeapBuf, apiPoint.c_str());
        strcpy(dataHeapBuf, data.c_str());

        APITalkRequest pkt{h, id, method, apiPointHeapBuf, dataHeapBuf};
        if (xQueueSend(apiTalksRequestQueue, &pkt, 0) != pdPASS) {
            free(apiPointHeapBuf);
            free(dataHeapBuf);
        }
    }
}

void ConnectivityServer::apiTalksWorker() {
    // TODO: Handle updates

    while(runAPITalksWorker){
        APITalkRequest pkt{};
        if ((apiTalksRequestQueue!= nullptr) and
            (xQueueReceive(apiTalksRequestQueue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE)) {
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
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if((millis() - lastWaterMarkPrint) >= 10000) {
            UBaseType_t freeWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("Connectivity API talks stack free: %u\n\r",
                          freeWords);
            lastWaterMarkPrint= millis();
        }
    }
}

void ConnectivityServer::requestApiTalk(char method, const std::string &point, const std::string &data) {
    appendToAPITalksRequestQueue(UINT16_MAX, UINT16_MAX, point, method, data);
}
