//
// Created by dkulpa on 20.08.2025.
//

#ifndef MGLIGHTFW_G2_CONNECTIVITY_H
#define MGLIGHTFW_G2_CONNECTIVITY_H

#include <Arduino.h>
#include "../new_bleln/BLELNClient.h"
#include "../new_bleln/BLELNServer.h"
#include "config.h"
#include "SuperString.h"
#include "DeviceConfig.h"
#include "WiFiManager.h"
#include <HTTPUpdate.h>

#define RECENTLY_HAS_BEEN_SERVER_PREFS_TAG  "rhbs"

class ConnectivityServer;
class ConnectivityClient;
class ConnectivityConfig;

class Connectivity {
public:
    enum class ConnectivityMode {ClientMode, ServerMode, ConfigMode};
    typedef std::function<void(int, int, int, const std::string &)> OnApiResponseCb;
    typedef std::function<void(ConnectivityMode)> RequestModeChangeCb;

    void start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences,
               const OnApiResponseCb &onApiResponse);
    void loop();
    void startAPITalk(const std::string& apiPoint, char method, const std::string& data); // Talk with API about me

private:
    Preferences *prefs;

    BLELNServer blelnServer;
    ConnectivityServer *conServer= nullptr;
    ConnectivityClient *conClient= nullptr;
    ConnectivityConfig *conConfig= nullptr;

    WiFiManager wiFiManager;

    // State
    ConnectivityMode conMode= ConnectivityMode::ClientMode;
};



#endif //MGLIGHTFW_G2_CONNECTIVITY_H
