//
// Created by dkulpa on 30.09.2025.
//

#ifndef MGLIGHTFW_CONNECTIVITYSERVER_H
#define MGLIGHTFW_CONNECTIVITYSERVER_H

#include "../bleln/BLELNServer.h"
#include "DeviceConfig.h"
#include "SuperString.h"
#include "config.h"

#include "WiFiManager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "caCertsBundle.h"
#include <HTTPUpdate.h>
#include "Connectivity.h"

#define BLELN_SERVER_SEARCH_INTERVAL_MS     (5*60000)   // 5 min

struct APITalkRequest {
    uint16_t h;
    uint16_t id;
    char method;
    char *apiPoint; // malloc/free
    char *data; // malloc/free
};

struct APITalkResponse {
    uint16_t h;
    uint16_t id;
    uint8_t errc;   // Error code: 0 - no error, 1 - WiFi error, 2 - HTTP Connect error, 3 - Server error
    uint16_t respCode;
    char *data;
};

class ConnectivityServer {
public:
    enum class ServerModeState {Init, Idle, OtherBLELNServerFound};

    ConnectivityServer(BLELNServer *blelnServer, DeviceConfig *deviceConfig, Preferences *preferences,
                       WiFiManager *wifiManager, Connectivity::OnApiResponseCb onApiResponse,
                       Connectivity::RequestModeChangeCb requestModeChange);
    void loop();
    void apiTalksWorker();
    void requestApiTalk(char method, const std::string &point, const std::string &data);
private:
    Preferences *prefs;
    DeviceConfig *config;

    BLELNServer *blelnServer;

    ServerModeState state;                            // State in server mode

    // Callbacks
    Connectivity::OnApiResponseCb oar; // On API Response callback
    Connectivity::RequestModeChangeCb rmc;

    void finish();
    void switchToClient();
    void onMessageReceived(uint16_t cliH, const std::string &msg);
    void handleAPIResponse();
    // Server mode variables
    unsigned long lastServerSearch= 0;

    // API Talk mathods
    void appendToAPITalksRequestQueue(uint16_t h, uint16_t id, const std::string& apiPoint, char method, const std::string &data);
    void appendToAPITalksResponseQueue(uint16_t h, uint16_t id, uint8_t errc, uint16_t respCode, const String& data);

    // API Talk variables
    bool runAPITalksWorker;
    QueueHandle_t apiTalksRequestQueue;
    QueueHandle_t apiTalksResponseQueue;

    // DEBUG
    unsigned long lastWaterMarkPrint=0;

    WiFiManager *wm;

    char updateCacheData[1024]{};
    uint16_t updateCacheSector=0;
    uint32_t updateCacheFwId=0;

    uint16_t updateReqSector=0;
    uint32_t updateReqFwId=0;
    uint16_t updateReqPage=0;

};


#endif //MGLIGHTFW_CONNECTIVITYSERVER_H
