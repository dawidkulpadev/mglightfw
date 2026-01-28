//
// Created by dkulpa on 1.10.2025.
//

#include "ConnectivityConfig.h"

ConnectivityConfig::ConnectivityConfig(BLELNServer *blelnServer, Preferences *preferences, DeviceConfig* deviceConfig) {
    this->blelnServer= blelnServer;
    prefs= preferences;
    config= deviceConfig;
    state= ConfigModeState::Start;
    WiFi.macAddress(mac);
}


void ConnectivityConfig::loop() {
    if(state==ConfigModeState::Start){
        Serial.println("Connectivity (Config): Start");

        blelnServer->start(prefs, BLE_NAME, BLELN_CONFIG_UUID);
        blelnServer->setOnMessageReceivedCallback([this](uint16_t cliH, const std::string &msg){
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
                    blelnServer->sendEncrypted(cliH, resp);
                }
            } else if(parts[0]=="$REBOOT"){
                rebootCalledAt= millis();
                rebootCalled= true;
            }
        });

        WiFiClass::mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.scanNetworks(true, false, false, 300);

        state = ConfigModeState::ServerTasking;
    } else if(state==ConfigModeState::ServerTasking){
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
            blelnServer->sendEncryptedToAll(("$WIFIL," + resStr));
        }

        if(rebootCalled){
            if(rebootCalledAt + 2000 < millis()){
                ConfigManager::writeDeviceConfig(prefs, this->config);
                esp_restart();
            }
        }
    }
}

uint8_t *ConnectivityConfig::getMAC() {
    return mac;
}