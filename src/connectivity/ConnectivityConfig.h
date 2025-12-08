//
// Created by dkulpa on 1.10.2025.
//

#ifndef MGLIGHTFW_CONNECTIVITYCONFIG_H
#define MGLIGHTFW_CONNECTIVITYCONFIG_H

#include "../new_bleln/BLELNServer.h"
#include "config.h"
#include "DeviceConfig.h"
#include "SuperString.h"
#include "WiFiManager.h"
#include "ConfigManager.h"

class ConnectivityConfig {
public:
    enum class ConfigModeState {Start, ServerTasking};

    explicit ConnectivityConfig(BLELNServer* blelnServer, Preferences *preferences, DeviceConfig* deviceConfig);
    void loop();
    uint8_t* getMAC();
private:
    BLELNServer *blelnServer;
    Preferences *prefs;
    DeviceConfig *config;

    ConfigModeState state;                           // State in config mode

    uint8_t mac[6]{};
    bool rebootCalled = false;                          // Reboot called by configuration app
    unsigned long rebootCalledAt= ULONG_MAX - 10000;    // When reboot was called
};


#endif //MGLIGHTFW_CONNECTIVITYCONFIG_H
