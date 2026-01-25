//
// Created by dkulpa on 20.08.2025.
//

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
        Serial.println("Connectivity - Start as client");
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








