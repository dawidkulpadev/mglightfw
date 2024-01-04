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
  if (https.begin(*client, api_url+"/get.php")) {  // HTTPS
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
            int ua= getUIntValue(payload, "\"UA\":");

            Serial.printf("Update available: %d\r\nDay config:\r\n\tDLI: %d\r\n\tDS: %d\r\n\tDE: %d\r\n\tSSD: %d\r\n\tSRD: %d\r\n",
                                ua, day->getDli(), day->getDs(), day->getDe(), day->getSsd(), day->getSrd());

            if(ua == 1){
                delay(200);
                upgrade(*client);
            }

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
        return text.substring(kpos + klen).toInt();
    } else {
        return -1;
    }
}

MGLightAPI::UpgradeResult MGLightAPI::upgrade(WiFiClientSecure& cli) {
    String url= api_url+"/upgrade.php";

    httpUpdate.rebootOnUpdate(true);
    httpUpdate.onProgress([](int done, int size){
        Serial.print("\rSystem upgrade: "+String( static_cast<int>((static_cast<double>(done)/static_cast<double>(size))*100.0) )+"%");
    });

    t_httpUpdate_return ret = httpUpdate.update(cli, url.c_str(), "",[this](HTTPClient *client) {
        client->addHeader("x-mg-auth-id", this->id);
        client->addHeader("x-mg-auth-pl", this->picklock);
        client->addHeader("x-mg-auth-uid", String(this->uid));
        client->addHeader("x-mg-fwv", String(fw_version));
        Serial.println("System upgrade start!");
    });

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            return UpgradeResult::Failed;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            return UpgradeResult::Failed;

        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            return UpgradeResult::Ok;
    }

    return UpgradeResult::Failed;
}
