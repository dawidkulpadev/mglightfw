//
// Created by dkulpa on 4.10.2025.
//

#ifndef MGLIGHTFW_WIFIMANAGER_H
#define MGLIGHTFW_WIFIMANAGER_H

#include <WiFi.h>
#include <WebServer.h>

#define WIFI_CONNECT_MAX_DURATION_MS        (15*1000ul)           // 15s
#define WIFI_NTP_MAX_RETIRES                1

class WiFiManager {
public:
    enum class WiFiState {Init, Connecting, NTPSyncing, NTPSyncFailed, Ready, ConnectFailed};
    void startConnect(const std::string &timezone, const std::string &wifiSSID, const std::string &wifiPsk);
    void stop();

    bool isConnected();
    bool isRunning();
    bool hasFailed();

private:
    WiFiState state= WiFiState::Init;
    uint32_t overallFails=0;
    uint8_t ntpRetriesCnt=0;

    bool runMainLoop=false;
    bool loopRunning=false;

    uint32_t timeSyncStartMs;
    uint32_t connectStartMs;

    std::string tz;
    std::string ssid;
    std::string psk;

    void loop();
};


#endif //MGLIGHTFW_WIFIMANAGER_H
