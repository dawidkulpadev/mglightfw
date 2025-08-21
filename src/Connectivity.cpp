//
// Created by dkulpa on 20.08.2025.
//

#include "Connectivity.h"

void Connectivity::start(int devMode) {
    dm= devMode;
}

bool Connectivity::syncWithNTP(const std::string &tz) {
    uint16_t reptCnt= 15;
    configTzTime(tz.c_str(), "pool.ntp.org");

    Serial.print(F("Waiting for NTP time sync: "));
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2) {
        delay(500);
        Serial.print(F("."));
        yield();
        nowSecs = time(nullptr);
        reptCnt--;
        if(reptCnt==0){
            Serial.println("Failed SNTP sync!");
            return false;
        }
    }

    Serial.println();
    struct tm timeinfo{};
    getLocalTime(&timeinfo);
    Serial.print(F("Current time: "));
    Serial.print(asctime(&timeinfo));

    return true;
}

void Connectivity::loop() {
    if(dm==DEVICE_MODE_CONFIG)
        loop_config();
    else
        loop_normal();
}

void Connectivity::startAPITalk() {

}

void Connectivity::loop_config() {

}

void Connectivity::loop_normal() {

}


