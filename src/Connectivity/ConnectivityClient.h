//
// Created by dkulpa on 30.09.2025.
//

#ifndef MGLIGHTFW_CONNECTIVITYCLIENT_H
#define MGLIGHTFW_CONNECTIVITYCLIENT_H

#include "BLELNClient.h"
#include "string"
#include "config.h"
#include "WiFiManager.h"
#include "SuperString.h"
#include "Connectivity.h"

#define CLIENT_SERVER_CHECK_INTERVAL        ((10+20+5)*1000)       // 10s + 20s
#define CLIENT_TIME_SYNC_INTERVAL           ((600)*1000)            // 10 min
#define WIFI_CONNECT_MAX_DURATION_MS        (15*1000)       // 15s
#define WIFI_NTP_MAX_RETIRES                1
#define BLE_REASON_MAX_CLIENTS              1 // TODO: Replace with real value

class ConnectivityClient {
public:
    ConnectivityClient(DeviceConfig *deviceConfig, Connectivity::OnApiResponseCb onApiResponse,
                       Connectivity::RequestModeChangeCb requestModeChange);

    enum class State {Init, Idle, ServerSearching, ServerChecking, ServerConnecting, ServerConnected,
        ServerNotFound, ServerConnectFailed, WaitingForHTTPResponse, HTTPResponseReceived, WiFiChecking, WiFiConnected, WiFiConnectFailed};
    enum class ConnectedFor {None, APITalk, TimeSync, Update};
    enum class WiFiStateMachineStates {None, Connecting, NTPSyncing, NTPSyncFailed, Ready, ConnectFailed};

    void loop();
    void startAPITalk(const std::string& apiPoint, char method, const std::string& data); // Talk with API about me
private:
    DeviceConfig *config;

    State state;                            // State in client mode
    BLELNClient blelnClient;

    // Callbacks
    Connectivity::OnApiResponseCb oar; // On API Response callback
    Connectivity::RequestModeChangeCb rmc;

    void onServerResponse(const std::string &msg);
    void onServerSearchResult(const NimBLEAdvertisedDevice* dev);
    void finish();
    void switchToServer();
    // Client mode variables
    unsigned long lastServerCheck=0;
    unsigned long lastTimeSync=0;
    ConnectedFor connectedFor;

    // WiFi methods
    bool syncWithNTP(const std::string &tz);
    // WiFi variables
    WiFiStateMachineStates wifiState;
    unsigned long wifi_timeSyncStart;
    unsigned long wifi_connectStart;                     // WiFi search and connecting started at
    uint8_t wifi_overallFails=0;                         // Count every WiFi connect fail TODO: Reset every day
    uint8_t ntpRetriesCnt=0;

    // My API Talk variables
    SemaphoreHandle_t meApiTalkMutex;
    bool meApiTalkRequested= false;
    std::string meApiTalkData;
    std::string meApiTalkPoint;
    char meApiTalkMethod='N';
};


#endif //MGLIGHTFW_CONNECTIVITYCLIENT_H
