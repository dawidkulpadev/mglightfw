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
    * Make auto timezone read                               []

  BUGS:
    *

*/

#include <Arduino.h>

#include <ctime>
#include <Ticker.h>

#include "PWMLed.h"
#include "Day.h"
#include "DeviceConfig.h"
#include "ConfigManager.h"
#include "InternalTempSensor.h"

#include "config.h"
#include "connectivity/Connectivity.h"

#define WIFI_RUN_INTERVAL       120
#define API_RUN_INTERVAL        600
#define DAY_UPDATE_INTERVAL     1
#define API_TALK_INTERVAL       60

int deviceMode;
Preferences prefs;
Connectivity connectivity;

PWMLed light(0, pinout_intensity, 200);
Day day;
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

void printHello(){
    uint64_t fmac= ESP.getEfuseMac();
    auto *mac= reinterpret_cast<uint8_t *>(&fmac);
    Serial.println("MioGiapicco Light Firmware  Copyright (C) 2023  Dawid Kulpa");
    Serial.println("This program comes with ABSOLUTELY NO WARRANTY;");
    Serial.println("This is free software, and you are welcome to redistribute it under certain conditions;");
    Serial.println("You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.");
    Serial.printf("Hardware code: %d, Firmware code: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  hw_id, fw_version, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

unsigned long lastWaterMarkPrint= 0;
bool runConnectivity= false;
void connectivityLoop(){
    while(runConnectivity){
        connectivity.loop();

        if(lastWaterMarkPrint+10000 < millis()) {
            UBaseType_t freeWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("Connectivity loop stack free: %u\n\r",
                          freeWords);
            lastWaterMarkPrint= millis();
        }
    }
}

int getUIntValue(const std::string &text, const std::string &key){
    size_t kpos= text.find(key);
    unsigned int klen= key.length();

    if(kpos != std::string::npos){
        try {
            return std::stoi(text.substr(kpos + klen));
        } catch (std::invalid_argument &e){
            return -2;
        } catch (std::out_of_range &e) {
            return -3;
        }
    } else {
        return -1;
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

    prefs.begin("mgld", false);

    bool buttonPressed= false;

    if(digitalRead(pinout_switch)==LOW){
        delay(25);
        if(digitalRead(pinout_switch)==LOW)
            buttonPressed= true;
    }

    if(buttonPressed or !ConfigManager::readDeviceConfig(&prefs, &config)){
        Serial.println("Device: Config mode");
        deviceMode= DEVICE_MODE_CONFIG;
    } else {
        Serial.println("Device: Normal mode");
    }

    //Setup WiFi
    connectivity.start(deviceMode, &config, &prefs, [](int id, int errc, int httpCode, const std::string &msg){
        if(errc==0 and httpCode==200) {
            int val;

            //Read DLI value
            val = getUIntValue(msg, "\"DLI\":");
            if (val >= 0)
                day.setDli(val);

            //Read DS value
            val = getUIntValue(msg, "\"DS\":");
            if (val >= 0)
                day.setDs(val);

            //Read DE value
            val = getUIntValue(msg, "\"DE\":");
            if (val >= 0)
                day.setDe(val);

            //Read SSD value
            val = getUIntValue(msg, "\"SSD\":");
            if (val >= 0)
                day.setSsd(val);

            //Read SRD value
            val = getUIntValue(msg, "\"SRD\":");
            if (val >= 0)
                day.setSrd(val);

            Serial.println("main - Day configuration received");
            Serial.printf("main - DS: %d, DE: %d, SSD: %d, SRD: %d, DLI: %d\r\n", day.getDs(), day.getDe(),
                          day.getSsd(), day.getSrd(), day.getDli());
            ConfigManager::writeDay(&prefs, &day);
        } else {
            Serial.println("main - API Talk failed");
        }
    });

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

    runConnectivity= true;
    xTaskCreatePinnedToCore([](void* arg){
                                connectivityLoop();
                                vTaskDelete(nullptr);
                            },
                            "conlp", 3000, nullptr, 5, nullptr, 1);
}
int lastAPI=-(API_RUN_INTERVAL/2);

uint32_t loopTicks=0;
uint32_t lastLedChange=0;
uint8_t ledState=0;
uint32_t lastServerTalk=0;


// NEVER BLOCK INSIDE!
void loop() {
    if(deviceMode==DEVICE_MODE_CONFIG){
        if((lastLedChange+5) <= loopTicks){
            ledState= !ledState;
            digitalWrite(pinout_sys_led, ledState);

            lastLedChange= loopTicks;
        }

        delay(10);
    } else {
        // Set pwm infill
        auto nowsse= static_cast<uint32_t>(time(nullptr));    // [seconds] since epoch
        float intensity= day.getSunIntensity(nowDayTime(), light.getIntensity());
        light.setIntensity(intensity);
        if(intensity>30.0) {
            digitalWrite(pinout_fan, HIGH);
        } else {
            digitalWrite(pinout_fan, LOW);
        }

        if(lastServerTalk+API_TALK_INTERVAL < nowsse){
            Serial.println("main - Request API Talk");
            float t= InternalTempSensor_read();
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char buf[100];
            sprintf(buf, "fv=%d&t=%d", fw_version, (int)t);
            connectivity.startAPITalk("light/get.php", 'P', mac, config.getPicklock(), buf);
            lastServerTalk= nowsse;
        }
    }
}

