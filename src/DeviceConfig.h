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

#ifndef UNTITLED_DEVICECONFIG_H
#define UNTITLED_DEVICECONFIG_H

#include <WString.h>
#include "bleln/BLELNBase.h"

#define DEVICE_CONFIG_ROLE_AUTO     '0'
#define DEVICE_CONFIG_ROLE_SERVER   '1'
#define DEVICE_CONFIG_ROLE_CLIENT   '2'

#define CONFIGMANAGER_KEY_PSK       "psk"
#define CONFIGMANAGER_KEY_SSID      "ssid"
#define CONFIGMANAGER_KEY_PICKLOCK  "picklock"
#define CONFIGMANAGER_KEY_UID       "uid"
#define CONFIGMANAGER_KEY_TIMEZONE  "tz"
#define CONFIGMANAGER_KEY_ROLE      "role"
#define CONFIGMANAGER_KEY_CERTSIGN  "certsign"

#define CONFIGMANAGER_KEY_DLI       "dli"
#define CONFIGMANAGER_KEY_DS        "ds"
#define CONFIGMANAGER_KEY_DE        "de"
#define CONFIGMANAGER_KEY_SSD       "ssd"
#define CONFIGMANAGER_KEY_SRD       "srd"


class DeviceConfig {
public:
    DeviceConfig();

    // Base
    std::string getSsid() const;
    std::string getPsk() const;
    std::string getTimezone() const;
    char getRole() const;

    void setSsid(std::string v);
    void setPsk(std::string v);
    void setTimezone(std::string v);
    void setRole(char role);

    // id
    std::string getUid() const;
    std::string getPicklock();
    const uint8_t* getCertSign() const;

    void setUid(std::string v);
    void setPicklock(std::string v);
    void setCertSignFromBase64(const std::string& b64);

    // cert
    const uint8_t* getManuPubKey() const;
    const uint8_t* getMyPrivateKey() const;
    const uint8_t* getMyPublicKey() const;

    // day
    void setDli(int v);
    void setDs(int v);
    void setDe(int v);
    void setSsd(int v);
    void setSrd(int v);

    int getDli() const;
    int getDs() const;
    int getDe() const;
    int getSsd() const;
    int getSrd() const;

    float getSunIntensity(uint32_t dayTime, float lastIntensity) const;

    bool loadConfig();
    void writeBaseConfig();
    void writeIdConfig();
    void writeDayConfig() const;

    void factoryReset();

private:
    // base
    std::string ssid;
    std::string psk;
    std::string tz;
    char role{};

    // id
    std::string uid;
    std::string picklock;
    uint8_t certSign[BLELN_MANU_SIGN_LEN]{};

    // cert
    uint8_t manuPubKey[BLELN_MANU_PUB_KEY_LEN]{};
    uint8_t myPrivateKey[BLELN_DEV_PRIV_KEY_LEN]{};
    uint8_t myPublicKey[BLELN_DEV_PUB_KEY_LEN]{};

    // day
    int dli=1000; //Daylight intensity (in min since 00:00)
    int ds{};   //Day start (in min since 00:00)
    int de{};   //Day end (in min since 00:00)
    int ssd{};  //Sunset duration (in min since 00:00)
    int srd{};  //Sunrise duration (in min since 00:00)
};


#endif //UNTITLED_DEVICECONFIG_H
