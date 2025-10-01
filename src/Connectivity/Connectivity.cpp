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
    if(devMode==DEVICE_MODE_CONFIG) {
        conConfig= new ConnectivityConfig(&blelnServer, preferences, devConfig);
        conMode = ConnectivityMode::ConfigMode;
    } else {
        conClient= new ConnectivityClient(devConfig, onApiResponse,
                                          [this](ConnectivityMode m){
            this->conMode= m;
        });
        conServer= new ConnectivityServer(&blelnServer, devConfig, preferences, onApiResponse,
                                          [this](ConnectivityMode m){
            this->conMode= m;
        });

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
    if(conMode==ConnectivityMode::ClientMode)
        conClient->startAPITalk(apiPoint, method, data);
    else if(conMode==ConnectivityMode::ServerMode)
        conServer->requestApiTalk(method, apiPoint, data);
}








