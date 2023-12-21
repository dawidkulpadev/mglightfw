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
#include "SystemUpgrader.h"

MGLightAPI::MGLightAPI(int uid, const char* picklock, Day *day) {
    this->day= day;
    this->uid= uid;

    uint8_t mac[6];
    char idBuf[13];
    WiFi.macAddress(mac);
    sprintf(idBuf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    id= String(idBuf);

    this->picklock= new char[strlen(picklock)+1];
    strcpy(this->picklock, picklock);
}

void MGLightAPI::talkWithServer() {
  bool success=false;

  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);

  client->setCACertBundle(rootca_crt_bundle_start);

  HTTPClient https;
  if (https.begin(*client, "https://dawidkulpa.pl/apis/miogiapicco/light/get.php")) {  // HTTPS
    char postBody[256];

    sprintf(postBody, MGLIGHTAPI_POST_DATA_FORMAT, id.c_str(), uid, picklock, fw_version);
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

            // Read upgrade allowed value
            val= getUIntValue(payload, "\"UA\"");
            if(val == 1){
                //Read the latest firmware version value
                int lfwv= getUIntValue(payload, "\"LFWV\"");
                if(lfwv!=fw_version)
                    SystemUpgrader::upgrade(*client, lfwv, id, picklock, uid);
            }

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


int MGLightAPI::getUIntValue(const String &text, const String &key){
    int kpos= text.indexOf(key);
    int vend= text.indexOf('&', kpos);
    unsigned int klen= key.length();

    if(kpos >= 0){
        if(vend>=0)
            return text.substring(kpos+klen+1,vend-1).toInt();
        else
            return text.substring(kpos+klen+1).toInt();
    } else {
        return -1;
    }
}
