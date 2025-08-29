//
// Created by dkulpa on 20.08.2025.
//

#ifndef MGLIGHTFW_G2_CONNECTIVITY_H
#define MGLIGHTFW_G2_CONNECTIVITY_H

#include <Arduino.h>
#include "BLELN/BLELNClient.h"
#include "BLELN/BLELNServer.h"
#include "BLEConfigurer.h"
#include "config.h"
#include "SuperString.h"
#include "WiFiManager.h"

#define CONNECTIVITY_ROLE_CLIENT    1
#define CONNECTIVITY_ROLE_SERVER    2

class Connectivity {
public:
    enum class StateMachineStates {Start, FirstServerSearch, ServerConnected, ServerNotFound, ServerTasking, ServerBondingError, Idle, LookingForServer};
    enum class WiFiStateMachineStates {None, Connecting, NTPSyncing, NTPSyncFailed, Ready, ConnecFailed};

    void start(uint8_t devMode, DeviceConfig *devConfig, Preferences *preferences);
    void loop();
    void startAPITalk(); // Talk with API about me

    uint8_t* getMAC();

private:
    uint8_t mac[6];
    BLELNServer blelnServer;
    BLELNClient blelnClient;
    BLEConfigurer bleConfig;
    StateMachineStates sms= StateMachineStates::Idle; // State machine state
    WiFiStateMachineStates wifiSms;
    Preferences *prefs;
    DeviceConfig *config;
    uint8_t dm; // Device mode
    uint8_t role;

    bool rebootCalled = false;
    unsigned long rebootCalledAt = ULONG_MAX - 10000;



    uint32_t timeSyncStart;

    void loop_config();
    void loop_normal();

    bool syncWithNTP(const std::string &tz);
};


#endif //MGLIGHTFW_G2_CONNECTIVITY_H
