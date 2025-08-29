/**
    MioGiapicco Light Firmware - Firmware for Light Device of MioGiapicco system
    Copyright (C) 2023  Dawid Kulpa

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Please feel free to contact me at any time by email <dawidkulpadev@gmail.com>

  FEATURES:
    - Cert store HTTPS connections
    - Creats PWM output 12V power rail for LED power and controll
    - Turns output on and off as specified in MioGiapicco Database for sepcific device
    - Makes fade slope as specified in MioGiapicco Database for specific device (sunrise and sunset simulation)
    - Connects to MioGiapicco Database with MioGiapicco Light API
    - Connects to WiFi specified in non volatile memory
    - Config mode for WiFi and MG Light API configuration
    - Configurable with MioGiapicco Home App (BLE connection)
    - Enter config mode if button is pressed on startup
    - Factory reset on veeeeery long button press (>15s)

  TODO:
    * Configure real time with BLE                          []
    * Implement OTA                                         []
    ** Make linear light gradient                           []

  BUGS:
    *

*/

#include <Arduino.h>

#include <ctime>

#include "PWMLed.h"
#include "Day.h"
#include "MGLightAPI.h"
#include "DeviceConfig.h"
#include "ConfigManager.h"
#include "BLEConfigurer.h"
#include "WiFiManager.h"

#include "config.h"
#include "BLELN/BLELNServer.h"
#include "BLELN/BLELNClient.h"
#include "Connectivity.h"

#define WIFI_RUN_INTERVAL       120
#define API_RUN_INTERVAL        600
#define DAY_UPDATE_INTERVAL     1
#define API_TALK_INTERVAL       600

bool timeisset=false;

int deviceMode;
Preferences prefs;
Connectivity connectivity;
WiFiManager wifiManager;
BLEConfigurer bleManager;

PWMLed light(0, pinout_intensity, 200);
Day day;
MGLightAPI *serverAPI;
std::string timezone;

//Read wifi configuration
DeviceConfig config;

Ticker configButtonTicker;
Ticker sunUpdateTicker;

void updateTicker(){

}



int nowDayTime(){
    struct tm timeinfo{};
    if(!getLocalTime(&timeinfo)){
        return 0;
    }

    time_t t;
    time(&t);

    return timeinfo.tm_hour*3600 + timeinfo.tm_min*60 + timeinfo.tm_sec;
}

// TODO: Move to connectivity
/*void setupConnectivity(int m, DeviceConfig &config){
    if(m == MODE_NORMAL) {
        Serial.println("Normal mode");
        esp_bt_controller_disable();
        wifiManager.initNormalMode(config.getSsid(), config.getPsk());

        serverAPI = new MGLightAPI(atoi(config.getUid()), config.getPicklock(), &day);
    } else if(m == MODE_CONFIG){
        Serial.println("Config mode");
        bleManager.start(wifiManager.getMAC(), &config, &light);
    }
}*/

void printHello(){
    Serial.println("MioGiapicco Light Firmware  Copyright (C) 2023  Dawid Kulpa");
    Serial.println("This program comes with ABSOLUTELY NO WARRANTY;");
    Serial.println("This is free software, and you are welcome to redistribute it under certain conditions;");
    Serial.println("You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.");
    Serial.printf("Hardware code: %d, Firmware code: %d\r\n", hw_id, fw_version);
}

#define FACTORY_RESET_BLINKS_CNT  6
#define FACTORY_RESET_BLINKS_INTERVAL 200

void factoryReset(){
    // Signal with led fast blinks
    for(int i=0; i<FACTORY_RESET_BLINKS_CNT; i++){
        digitalWrite(pinout_sys_led, LOW);
        delay(FACTORY_RESET_BLINKS_INTERVAL);
        digitalWrite(pinout_sys_led, HIGH);
        delay(FACTORY_RESET_BLINKS_INTERVAL);
    }

    // Remove WiFi config file
    ConfigManager::clearWifiConfig(&prefs);
    esp_restart();
}

int btnPressCnt=0;
void countButtonPressPeriod(){
    // Count button press passes
    if(digitalRead(pinout_switch)==LOW){
        btnPressCnt++;
    } else {
        btnPressCnt=0;
    }

    // Factory Reset if button pressed for over 15s (15 passes for 1000ms loop delay)
    if(btnPressCnt>15){
        factoryReset();
    }
}

void setup() {
    deviceMode= DEVICE_MODE_NORMAL;

    //Setup leds
    pinMode(pinout_switch, INPUT_PULLUP);
    pinMode(pinout_sys_led, OUTPUT);
    pinMode(pinout_fan, OUTPUT);
    digitalWrite(pinout_fan, LOW);
    digitalWrite(pinout_sys_led, LOW);

    Serial.begin(115200);
    Serial.println("");
    delay(1000);
    printHello();
    delay(5000);



    bool buttonPressed= false;

    if(digitalRead(pinout_switch)==LOW){
        delay(25);
        if(digitalRead(pinout_switch)==LOW)
            buttonPressed= true;
    }

    if(buttonPressed || !ConfigManager::readWifi(&prefs, &config)){
        Serial.println("Device: Config mode");
        deviceMode= DEVICE_MODE_CONFIG;
    } else {
        Serial.println("Device: Normal mode");
    }

    //Setup WiFi
    //setupConnectivity(mode, config);
    connectivity.start(deviceMode, &config, &prefs);


    light.start();
    if(deviceMode==DEVICE_MODE_NORMAL) {
        Serial.println("Reading Day config file...");
        if(!ConfigManager::readDay(&prefs, &day))
            Serial.println("Day config file not found :(");
        Serial.printf("Day config:\r\n\tDLI: %d\r\n\tDS: %d\r\n\tDE: %d\r\n\tSSD: %d\r\n\tSRD: %d\r\n",
                      day.getDli(), day.getDs(), day.getDe(), day.getSsd(), day.getSrd());
        configButtonTicker.attach(1, countButtonPressPeriod);
    } else {
        light.setIntensity(0);
    }

    digitalWrite(pinout_sys_led, HIGH);
}

int lastWifi=-WIFI_RUN_INTERVAL;
int lastAPI=-(API_RUN_INTERVAL/2);

uint32_t loopTicks=0;
uint32_t lastLedChange=0;
uint8_t ledState=0;
std::string wifis;
uint32_t lastServerTalk=0;

// NEVER BLOCK INSIDE!
void loop() {
    connectivity.loop();

    if(deviceMode==DEVICE_MODE_CONFIG){
        if((lastLedChange+5) <= loopTicks){
            ledState= !ledState;
            digitalWrite(pinout_sys_led, ledState);

            lastLedChange= loopTicks;
        }

        delay(10);
    } else {
        // Set pwm infill
        int nowsse= static_cast<int>(time(nullptr));    // [seconds] since epoch
        float intensity= day.getSunIntensity(nowDayTime(), light.getIntensity());
        Serial.printf("Intensity: %f\r\n", intensity);
        light.setIntensity(intensity);
        if(intensity>50.0) {
            digitalWrite(pinout_fan, HIGH);
        } else {
            digitalWrite(pinout_fan, LOW);
        }

        if(lastServerTalk+API_TALK_INTERVAL < nowsse){
            connectivity.startAPITalk();
        }
    }

    // TODO: Move to connectivity
    /*
    } */
}

