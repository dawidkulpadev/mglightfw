//
// Created by dkulpa on 20.08.2025.
//

#include <vector>
#include "Connectivity.h"
#include "esp_heap_caps.h"

void Connectivity::start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences) {
    prefs= preferences;
    config= devConfig;
    dm= devMode;
    sms= StateMachineStates::Start;
    wifiSms = WiFiStateMachineStates::None;
}

void Connectivity::loop() {
    if(dm==DEVICE_MODE_CONFIG)
        loop_config();
    else
        loop_normal();
}

void Connectivity::startAPITalk() {}

// TODO: Handle comas ','

void Connectivity::loop_config() {
    if(sms == StateMachineStates::Start){
        Serial.println("Connectivity (Config): Start");
        WiFi.macAddress(mac);
        blelnServer.start(prefs);
        blelnServer.setOnMessageReceivedCallback([this](BLELNConnCtx* cx, const std::string &msg){
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
                    blelnServer.sendEncrypted(cx, resp);
                }
            } else if(parts[0]=="$REBOOT"){
                rebootCalledAt= millis();
                rebootCalled= true;
            }
        });
        //bleConfig.start(prefs, mac, config, nullptr);
        WiFiClass::mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.scanNetworks(true, false, false, 300);

        sms = StateMachineStates::ServerTasking;
    } else if(sms == StateMachineStates::ServerTasking){
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

        if(rebootCalled){
            if(rebootCalledAt+2000 < millis()){
                NimBLEDevice::deinit(true);
                esp_restart();
            }
        }
    }
}

void Connectivity::loop_normal() {
    if(sms == StateMachineStates::Start){
        Serial.println("Connectivity (Normal): Start");
        sms= StateMachineStates::FirstServerSearch;
        Serial.println("Connectivity (Normal): Client start and looking for server");
        blelnClient.start();
        blelnClient.startServerSearch(5000, [this](bool success){
            if(success){
                this->sms = StateMachineStates::ServerConnected;
            } else {
                this->sms= StateMachineStates::ServerNotFound;
            }
        });
        // On start -   look for server for 5s - if not found become server;
        //              if found become client
    } else if(sms == StateMachineStates::FirstServerSearch){

    } else if(sms == StateMachineStates::ServerConnected){
        // TODO: Add timeout for BLE connect
        if(blelnClient.isConnected() and !blelnClient.hasDiscoveredClient()){
            if(!blelnClient.discover()){
                Serial.println("Connectivity (Normal): Discover fail!");
                delay(1000);
                sms= StateMachineStates::ServerBondingError;
                return;
            }
            if(!blelnClient.handshake()){
                Serial.println("Connectivity (Normal): Handshake fail!");
                delay(1000);
                sms= StateMachineStates::ServerBondingError;
                return;
            }
            Serial.println("Connectivity (Normal): Discover and handshake ok");
            sms = StateMachineStates::ServerTasking;
        }
    } else if(sms == StateMachineStates::ServerNotFound){
        Serial.println("Connectivity (Normal): Server not found. Becoming server...");
        // Become server
        blelnClient.stop();
        blelnServer.start(prefs);
        // Init WiFi
        WiFi.begin(config->getSsid(), config->getPsk());
        sms = StateMachineStates::ServerTasking;
        wifiSms = WiFiStateMachineStates::Connecting;
    } else if(sms == StateMachineStates::ServerTasking){
        if(wifiSms==WiFiStateMachineStates::Connecting){
            if(WiFi.isConnected()){
                syncWithNTP(config->getTimezone());
                Serial.print(F("Connectivity (Normal): Waiting for NTP time sync..."));
                wifiSms= WiFiStateMachineStates::NTPSyncing;
            }
        } else if(wifiSms==WiFiStateMachineStates::NTPSyncing){ // Wait for time sync with NTP
            time_t nowSecs = time(nullptr);
            if(nowSecs < (60*60*24*365*30)){ // 60s * 60m * 24h * 365days * 30years
                if(timeSyncStart+(15*1000) < millis()){ // Wait 15s
                    // TODO: Make NTP sync retry
                    Serial.println("Connectivity (Normal): Time sync failed! (inf loop)");
                    wifiSms= WiFiStateMachineStates::NTPSyncFailed;
                }
            } else {
                struct tm timeinfo{};
                getLocalTime(&timeinfo);
                Serial.print(F("Connectivity (Normal): Time synced - "));
                Serial.println(asctime(&timeinfo));
                wifiSms= WiFiStateMachineStates::Ready;
            }
        } else if(wifiSms==WiFiStateMachineStates::Ready){
            // TODO: Forward my own API Talk if requested

            // TODO: Forward others API Talks if requested
        }
    }
}

bool Connectivity::syncWithNTP(const std::string &tz) {
    uint16_t reptCnt= 15;
    configTzTime(tz.c_str(), "pool.ntp.org");
    timeSyncStart= millis();

    return true;
}

uint8_t *Connectivity::getMAC() {
    return mac;
}
