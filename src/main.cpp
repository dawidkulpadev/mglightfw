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
#include "BLEManager.h"
#include "WiFiManager.h"


#define SYS_LED_PIN   5
#define INTENSITY_PIN 4
#define SWITCH_PIN    10
#define PHOTODIODE_PIN 0

#define WIFI_RUN_INTERVAL       120
#define API_RUN_INTERVAL        600
#define DAY_UPDATE_INTERVAL     1

#define MODE_CONFIG             1
#define MODE_NORMAL             2

bool timeisset=false;

int mode;

WiFiManager wifiManager;
BLEManager bleManager;

PWMLed light(0, INTENSITY_PIN, 2000);
Day day;
MGLightAPI *serverAPI;
std::string timezone;

Ticker configButtonTicker;

bool syncWithNTP(const std::string &tz) {
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

int nowTime(){
    struct tm timeinfo{};
    if(!getLocalTime(&timeinfo)){
        return 0;
    }

    return timeinfo.tm_hour*3600 + timeinfo.tm_min*60 + timeinfo.tm_sec;
}

void setupConnectivity(int mode, DeviceConfig &config){
    if(mode==MODE_NORMAL) {
        Serial.println("Normal mode");
        esp_bt_controller_disable();
        wifiManager.initNormalMode(config.getSsid(), config.getPsk());

        serverAPI = new MGLightAPI(atoi(config.getUid()), config.getPicklock(), &day);
    } else if(mode==MODE_CONFIG){
        Serial.println("Config mode");
        bleManager.start(wifiManager.getMAC(), &config);
    }
}

void printHello(){
    Serial.println("MioGiapicco Light Firmware  Copyright (C) 2023  Dawid Kulpa");
    Serial.println("This program comes with ABSOLUTELY NO WARRANTY;");
    Serial.println("This is free software, and you are welcome to redistribute it under certain conditions;");
    Serial.println("You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.");
}

#define FACTORY_RESET_BLINKS_CNT  6
#define FACTORY_RESET_BLINKS_INTERVAL 200

void factoryReset(){
    // Signal with led fast blinks
    for(int i=0; i<FACTORY_RESET_BLINKS_CNT; i++){
        digitalWrite(SYS_LED_PIN, LOW);
        delay(FACTORY_RESET_BLINKS_INTERVAL);
        digitalWrite(SYS_LED_PIN, HIGH);
        delay(FACTORY_RESET_BLINKS_INTERVAL);
    }

    // Remove WiFi config file
    ConfigManager::clearWifiConfig();
    esp_restart();
}

int btnPressCnt=0;
void countButtonPressPeriod(){
    // Count button press passes
    if(digitalRead(SWITCH_PIN)==LOW){
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
    mode= MODE_NORMAL;

    //Setup leds
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    pinMode(SYS_LED_PIN, OUTPUT);
    pinMode(PHOTODIODE_PIN, OUTPUT);
    digitalWrite(PHOTODIODE_PIN, LOW);
    digitalWrite(SYS_LED_PIN, LOW);

    Serial.begin(115200);
    Serial.println("");
    delay(1000);
    printHello();
    delay(5000);

    //Read wifi configuration
    ConfigManager::init();
    DeviceConfig config;

    bool buttonPressed= false;

    if(digitalRead(SWITCH_PIN)==LOW){
        delay(25);
        if(digitalRead(SWITCH_PIN)==LOW)
            buttonPressed= true;
    }

    if(buttonPressed || !ConfigManager::readWifi(&config)){
        mode= MODE_CONFIG;
    } else {
        Serial.println("Reading timezone...");
        timezone= std::string(config.getTimezone());
        Serial.println(timezone.c_str());
    }

    //Setup WiFi
    setupConnectivity(mode, config);

    //Setup "threads"
    if(mode==MODE_NORMAL) {
        light.start();
        Serial.println("Reading Day config file...");
        if(!ConfigManager::readDay(&day))
            Serial.println("Day config file not found :(");
        Serial.printf("Day config:\r\n\tDLI: %d\r\n\tDS: %d\r\n\tDE: %d\r\n\tSSD: %d\r\n\tSRD: %d\r\n",
                      day.getDli(), day.getDs(), day.getDe(), day.getSsd(), day.getSrd());
        configButtonTicker.attach(1, countButtonPressPeriod);
    } else {
        // Config mode
    }

    digitalWrite(SYS_LED_PIN, HIGH);
}

int lastWifi=-WIFI_RUN_INTERVAL;
int lastAPI=-(API_RUN_INTERVAL/2);


void loop() {
    if(mode==MODE_CONFIG){
        delay(500);
        digitalWrite(SYS_LED_PIN, HIGH);
        delay(500);
        digitalWrite(SYS_LED_PIN, LOW);
    } else {
        delay(200);

        // Set pwm infill
        int nows= time(nullptr);    // [seconds]
        float intensity= day.getIntensity(nowTime());
        light.set(intensity);
        if(intensity>50.0){
            digitalWrite(PHOTODIODE_PIN, HIGH);
        } else {
            digitalWrite(PHOTODIODE_PIN, LOW);
        }

        //
        if(lastWifi+WIFI_RUN_INTERVAL < nows){
            lastWifi = nows;
            if(WiFi.isConnected()) {
                Serial.println("WiFi Connected...");
                if(!timeisset){
                    timeisset=syncWithNTP(timezone);
                    nows= time(nullptr);
                }
            } else {
                lastWifi-= WIFI_RUN_INTERVAL-10;
                Serial.println("WiFi Disconnected...");
            }
        }

        if(lastAPI+API_RUN_INTERVAL < nows){
            //if(timeisset){
            digitalWrite(SYS_LED_PIN, LOW);
            if(WiFi.isConnected()){
                serverAPI->talkWithServer();
            } else {
                lastAPI-= API_RUN_INTERVAL-10;
            }
            digitalWrite(SYS_LED_PIN, HIGH);
            //} else {
            //Serial.println("Real time not set yet...");
            //}

            lastAPI= nows;
        }
    }
}
  