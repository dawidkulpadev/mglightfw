//
// Created by dkulpa on 09.12.23.
//

#include "SystemUpgrader.h"

SystemUpgrader::Result SystemUpgrader::upgrade(WiFiClientSecure &cli, int v, const String &id, const String &picklock, int uid) {
    if((v/10000) != hw_version )
        return SystemUpgrader::Result::HwMismatch;

    std::string url= "https://dawidkulpa.pl/apis/miogiapicco/light/";

    httpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = httpUpdate.update(cli, url.c_str(), "",[&id, &picklock, uid](HTTPClient *client) {
        client->addHeader("x-mg-auth-id", id);
        client->addHeader("x-mg-auth-pl", picklock);
        client->addHeader("x-mg-auth-uid", String(uid));
    });


    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            return SystemUpgrader::Result::Failed;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            return SystemUpgrader::Result::Failed;

        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            return SystemUpgrader::Result::Ok;
    }

    return SystemUpgrader::Result::Failed;
}
