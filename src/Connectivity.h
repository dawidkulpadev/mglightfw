//
// Created by dkulpa on 20.08.2025.
//

#ifndef MGLIGHTFW_G2_CONNECTIVITY_H
#define MGLIGHTFW_G2_CONNECTIVITY_H

#include <Arduino.h>
#include "BLELNClient.h"
#include "BLELNServer.h"
#include "config.h"
#include "SuperString.h"
#include "WiFiManager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "caCertsBundle.h"
#include <HTTPUpdate.h>

#define CONNECTIVITY_ROLE_CLIENT    1
#define CONNECTIVITY_ROLE_SERVER    2

#define WIFI_CONNECT_MAX_DURATION_MS        (15*1000)       // 15s
#define WIFI_NTP_MAX_RETIRES                1
#define BLELN_SERVER_SEARCH_INTERVAL_MS     (5*60000)   // 5 min
#define BLE_REASON_MAX_CLIENTS              1 // TODO: Replace with real value
#define CLIENT_SERVER_CHECK_INTERVAL        ((10+20+5)*1000)       // 10s + 20s

/**
 * Workflow:
 * 1) Check if BLELN server in range                                | +
 *   a. Yes                                                         |
 *     1) Become client                                             |
 *     2) Once per x min check if BLELN server in range             |
 *       a. No                                                      |
 *         1) Check if WiFi in range                                |
 *           a. Yes                                                 |
 *             1) Become server                                     |
 *
 *   b. No                                                          | +
 *     1) Check if WiFi in range                                    | +
 *       a. Yes                                                     | +
 *         1) Become server                                         | +
 *         2) Once per x min check if other BLELN server in range   |
 *           a. Yes                                                 |
 *             1) Become client                                     |
 *       b. No                                                      | +
 *         1) Become client                                         |
 */

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

class Connectivity {
public:
    enum class ConnectivityMode {ClientMode, ServerMode, ConfigMode};
    enum class ConfigModeState {Start, ServerTasking};
    enum class WiFiStateMachineStates {None, Connecting, NTPSyncing, NTPSyncFailed, Ready, ConnectFailed};
    enum class ServerModeState {Init, Idle, OtherBLELNServerFound};
    enum class ClientModeState {Init, Idle, ServerSearching, ServerChecking, ServerConnecting, ServerConnected,
            ServerNotFound, ServerConnectFailed, WaitingForHTTPResponse, WiFiChecking, WiFiConnected, WiFiConnectFailed};

    void start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences, std::function<void(int, int, int, const std::string &msg)> onApiResponse);
    void loop();
    void startAPITalk(const std::string& apiPoint, char method, const std::string& data); // Talk with API about me

    void apiTalksWorker();
    uint8_t* getMAC();
private:
    // BLELN tools
    BLELNServer blelnServer;
    BLELNClient blelnClient;

    // External data/objects
    uint8_t mac[6];
    Preferences *prefs;
    DeviceConfig *config;

    // Callbacks
    std::function<void(int id, int errc, int httpCode, const std::string &msg)> oar; // On API Response callback

    // State
    ConnectivityMode conMode;                         // Connectivity state - Init, Server, Client
    ConfigModeState cfmState;                           // State in config mode
    ServerModeState smState;                            // State in server mode
    ClientModeState cmState;                            // State in client mode

    // WiFi methods
    bool syncWithNTP(const std::string &tz);
    // WiFi variables
    WiFiStateMachineStates wifiState;
    unsigned long wifi_timeSyncStart;
    unsigned long wifi_connectStart;                     // WiFi search and connecting started at
    uint8_t wifi_overallFails=0;                         // Count every WiFi connect fail TODO: Reset every day
    uint8_t ntpRetriesCnt=0;

    // Config mode methods
    void conf_loop();
    // Config mode variables
    bool conf_rebootCalled = false;                          // Reboot called by configuration app
    unsigned long conf_rebootCalledAt= ULONG_MAX - 10000;    // When reboot was called

    // Server mode methods
    void ser_loop();
    void ser_finish();
    void ser_switchToClient();
    void ser_onMessageReceived(uint16_t cliH, const std::string &msg);
    // Server mode variables
    unsigned long ser_lastServerSearch= 0;

    // Client mode methods
    void cli_loop();
    void cli_onServerResponse(const std::string &msg);
    void cli_onServerSearchResult(const NimBLEAdvertisedDevice* dev);
    void cli_finish();
    void cli_switchToServer();
    // Client mode variables
    unsigned long cli_lastServerCheck=0;


    // API Talk mathods
    void appendToAPITalksRequestQueue(uint16_t h, uint16_t id, const std::string& apiPoint, char method, const std::string &data);
    void appendToAPITalksResponseQueue(uint16_t h, uint16_t id, uint8_t errc, uint16_t respCode, const String& data);
    // API Talk variables
    bool runAPITalksWorker;
    QueueHandle_t apiTalksRequestQueue;
    QueueHandle_t apiTalksResponseQueue;

    // My API Talk variables
    SemaphoreHandle_t meApiTalkMutex;
    bool meApiTalkRequested= false;
    std::string meApiTalkData;
    std::string meApiTalkPoint;
    char meApiTalkMethod;

    // DEBUG
    unsigned long lastWaterMarkPrint;
};


#endif //MGLIGHTFW_G2_CONNECTIVITY_H
