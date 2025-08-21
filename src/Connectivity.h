//
// Created by dkulpa on 20.08.2025.
//

#ifndef MGLIGHTFW_G2_CONNECTIVITY_H
#define MGLIGHTFW_G2_CONNECTIVITY_H


#include "BLELN/BLELNClient.h"
#include "BLELN/BLELNServer.h"
#include "config.h"

class Connectivity {
public:
    enum class StateMachineStates {Idle, LookingForServer};

    bool syncWithNTP(const std::string &tz);
    void start(int devMode);
    void loop();
    void startAPITalk(); // Talk with API about me

private:
    int dm; // Device mode

    void loop_config();
    void loop_normal();

    BLELNServer blelnServer;
    BLELNClient blelnClient;
    StateMachineStates sms= StateMachineStates::Idle; // State machine state
};


#endif //MGLIGHTFW_G2_CONNECTIVITY_H
