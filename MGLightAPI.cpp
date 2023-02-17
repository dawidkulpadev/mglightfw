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
*/

#include "MGLightAPI.h"

// Fingerprint for demo URL, expires on June 10, 2023, needs to be updated well before this date
const uint8_t fingerprint[20] = {0x96, 0x71, 0xFB, 0xF6, 0x02, 0x18, 0x87, 0x9A, 0xCC, 0x4C, 0xF8, 0x0C, 0x7E, 0xC1, 0x9A, 0x0D, 0xB5, 0xFC, 0xD9, 0x83};

MGLightAPI::MGLightAPI(int uid, const char* picklock, Day *day) {
    this->day= day;
    this->uid= uid;

    this->picklock= new char[strlen(picklock)+1];
    strcpy(this->picklock, picklock);
}

void MGLightAPI::talkWithServer() {
  bool success=false;
  uint8_t mac[6];
  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);

  client->setCACertBundle(rootca_crt_bundle_start);

  HTTPClient https;

  if (https.begin(*client, "https://dawidkulpa.pl/apis/miogiapicco/light/get.php")) {  // HTTPS
    char postBody[256];
    WiFi.macAddress(mac);
    sprintf(postBody, MGLIGHTAPI_POST_DATA_FORMAT, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], uid, picklock);
    // start connection and send HTTPS header

    Serial.println(postBody);
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = https.POST(postBody);

    // httpCode will be negative on error
    if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled

        if (httpCode == 200) {
            String payload = https.getString();
            int val;

            //Read DLI value
            val= getUIntValue(payload, "\"DLI\":");
            if(val>=0)
                day->setDli(val);

            //Read DS value
            val= getUIntValue(payload, "\"DS\":");
            if(val>=0)
                day->setDs(val);

            //Read DE value
            val= getUIntValue(payload, "\"DE\":");
            if(val>=0)
                day->setDe(val);

            //Read SSD value
            val= getUIntValue(payload, "\"SSD\":");
            if(val>=0)
                day->setSsd(val);

            //Read SRD value
            val= getUIntValue(payload, "\"SRD\":");
            if(val>=0)
                day->setSrd(val);
                

            Serial.printf("Day config:\r\n\tDLI: %d\r\n\tDS: %d\r\n\tDE: %d\r\n\tSSD: %d\r\n\tSRD: %d\r\n",
                                day->getDli(), day->getDs(), day->getDe(), day->getSsd(), day->getSrd());

            ConfigManager::writeDay(day);
            success = true;
        } else {
            String payload = https.getString();
            Serial.println("HTTP Code not OK");
            Serial.println(payload);
            Serial.println(httpCode);
        }
    } else {
        Serial.printf("[HTTPS] POST... failed, error: %s\n", HTTPClient::errorToString(httpCode).c_str());
    }

    https.end();
  }

  if(!success){
    Serial.println("Server connection failed! Reading last known configuration");
    if(!ConfigManager::readDay(day)){
      Serial.println("No Day config file :(");
    }
  }
}

int MGLightAPI::getUIntValue(const String &text, char* key){
    const char* ctext= text.c_str();
    char* s= strstr(ctext, key);
    int slen= strlen(key);
    if(s!=nullptr){
        return atoi( (s+slen));
    } else {
        return -1;
    }
}

int MGLightAPI::getDayTimeValue(const String &text, char* key){
    const char* ctext= text.c_str();
    char* hs= strstr(ctext, key);
    int hslen= strlen(key);

    if(hs!=nullptr){
        char* ms= strstr( (char*)(hs+hslen+1), ":");
        if(ms!=nullptr){
            char* ss= strstr( (char*)(ms+1), ":");
            if(ss!=nullptr){
                int h= atoi( (hs+hslen+1) );
                int m= atoi( (ms+1) );
                int s= atoi( (ss+1) );

                return s+m*60+h*3600;
            }

            return -3;
        } else {
            return -2;
        }
    } else {
        return -1;
    }
}
