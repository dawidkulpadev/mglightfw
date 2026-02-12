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

#include "Arduino.h"
#include "DeviceConfig.h"
#include "Preferences.h"
#include "bleln/Encryption.h"

DeviceConfig::DeviceConfig() = default;

std::string DeviceConfig::getSsid() const {
    return ssid;
}

std::string DeviceConfig::getPsk() const {
    return psk;
}

std::string DeviceConfig::getTimezone() const {
    return tz;
}

char DeviceConfig::getRole() const {
    return role;
}

void DeviceConfig::setSsid(std::string v) {
    ssid= std::move(v);
}

void DeviceConfig::setPsk(std::string v) {
    psk= std::move(v);
}

void DeviceConfig::setTimezone(std::string v) {
    tz= std::move(v);
}

void DeviceConfig::setRole(char v) {
    role= v;
}

std::string DeviceConfig::getUid() const {
    return uid;
}

std::string DeviceConfig::getPicklock() {
    return picklock;
}

const uint8_t *DeviceConfig::getCertSign() const {
    return certSign;
}

void DeviceConfig::setUid(std::string v) {
    uid= std::move(v);
}

void DeviceConfig::setPicklock(std::string v) {
    picklock= std::move(v);
}

void DeviceConfig::setCertSignFromBase64(const std::string& b64) {
    Encryption::base64Decode(b64, certSign, BLELN_MANU_SIGN_LEN);
}

const uint8_t *DeviceConfig::getManuPubKey() const {
    return manuPubKey;
}

const uint8_t *DeviceConfig::getMyPrivateKey() const {
    return myPrivateKey;
}

const uint8_t *DeviceConfig::getMyPublicKey() const {
    return myPublicKey;
}



bool DeviceConfig::loadConfig() {
    Preferences prefs;

    // Read Base settings
    prefs.begin("base");
    ssid= prefs.getString(CONFIGMANAGER_KEY_SSID, "").c_str();
    psk= prefs.getString(CONFIGMANAGER_KEY_PSK, "").c_str();
    tz= prefs.getString(CONFIGMANAGER_KEY_TIMEZONE, "").c_str();
    role= prefs.getChar(CONFIGMANAGER_KEY_ROLE, '0');
    prefs.end();

    prefs.begin("id");
    uid= prefs.getString(CONFIGMANAGER_KEY_UID, "-1").c_str();
    prefs.getBytes("cert_sign", certSign, BLELN_MANU_SIGN_LEN);
    picklock= prefs.getString(CONFIGMANAGER_KEY_PICKLOCK, "").c_str();
    prefs.end();

    prefs.begin("cert");
    prefs.getBytes("manu_pub", manuPubKey, BLELN_MANU_PUB_KEY_LEN);
    prefs.getBytes("dev_priv", myPrivateKey, BLELN_DEV_PRIV_KEY_LEN);
    prefs.getBytes("dev_pub", myPublicKey, BLELN_DEV_PUB_KEY_LEN);
    prefs.end();

    prefs.begin("day");
    dli= prefs.getInt(CONFIGMANAGER_KEY_DLI, 0);
    ds= prefs.getInt(CONFIGMANAGER_KEY_DS, 0);
    de= prefs.getInt(CONFIGMANAGER_KEY_DE, 0);
    ssd= prefs.getInt(CONFIGMANAGER_KEY_SSD, 0);
    srd= prefs.getInt(CONFIGMANAGER_KEY_SRD, 0);
    prefs.end();

    return true;
}

void DeviceConfig::setDli(int v) {
    dli= v;
}

void DeviceConfig::setDs(int v) {
    ds= v;
}

void DeviceConfig::setDe(int v) {
    de= v;
}

void DeviceConfig::setSsd(int v) {
    ssd= v;
}

void DeviceConfig::setSrd(int v) {
    srd= v;
}

int DeviceConfig::getDli() const {
    return dli;
}

int DeviceConfig::getDs() const {
    return ds;
}

int DeviceConfig::getDe() const {
    return de;
}

int DeviceConfig::getSsd() const {
    return ssd;
}

int DeviceConfig::getSrd() const {
    return srd;
}

float DeviceConfig::getSunIntensity(uint32_t dayTime, float lastIntensity) const {
    double perc;
    int mins= dayTime/60;

    double lastPerc= (lastIntensity*10.0)/dli;

    if(mins >= (ds+srd) && mins<=(de-ssd)){ // Full day
        perc= 1.0;
    } else if(mins>ds && mins<(ds+srd)){ // Sunrise
        double a= 1.0/(double)srd;
        double b= -a*(double)ds;

        perc= (a*(double)mins)+b;

        if(perc<lastPerc){
            perc= lastPerc;
        }
    } else if(mins>(de-ssd) && mins<de) { // Sunset
        double a= -1.0/(double)ssd;
        double b= -a*(double)de;

        perc= (a*(double)mins)+b;

        if(perc>lastPerc){
            perc= lastPerc;
        }
    } else { // Night
        perc= 0.0;
    }

    //intensityValid= true;
    return perc*(double)dli/10.0;
}

void DeviceConfig::writeBaseConfig() {
    Preferences prefs;

    prefs.begin("base");
    prefs.putString(CONFIGMANAGER_KEY_SSID, ssid.c_str());
    prefs.putString(CONFIGMANAGER_KEY_PSK, psk.c_str());
    prefs.putString(CONFIGMANAGER_KEY_TIMEZONE, tz.c_str());
    prefs.putChar(CONFIGMANAGER_KEY_ROLE, role);
    prefs.end();
}

void DeviceConfig::writeIdConfig() {
    Preferences prefs;

    prefs.begin("id");
    prefs.putString(CONFIGMANAGER_KEY_UID, uid.c_str());
    prefs.putBytes("cert_sign", certSign, BLELN_MANU_SIGN_LEN);
    prefs.putString(CONFIGMANAGER_KEY_PICKLOCK, picklock.c_str());
    prefs.end();
}

void DeviceConfig::writeDayConfig() const {
    Preferences prefs;

    prefs.begin("day");
    prefs.putInt(CONFIGMANAGER_KEY_DLI, dli);
    prefs.putInt(CONFIGMANAGER_KEY_DS, ds);
    prefs.putInt(CONFIGMANAGER_KEY_DE, de);
    prefs.putInt(CONFIGMANAGER_KEY_SSD, ssd);
    prefs.putInt(CONFIGMANAGER_KEY_SRD, srd);
    prefs.end();
}

void DeviceConfig::factoryReset() {
    Preferences prefs;

    prefs.begin("base");
    prefs.remove(CONFIGMANAGER_KEY_SSID);
    prefs.remove(CONFIGMANAGER_KEY_PSK);
    prefs.remove(CONFIGMANAGER_KEY_TIMEZONE);
    prefs.remove(CONFIGMANAGER_KEY_ROLE);
    prefs.end();

    prefs.begin("cert");
    uint8_t factoryCertSign[BLELN_MANU_SIGN_LEN];
    prefs.getBytes("pc_sign", factoryCertSign, BLELN_MANU_SIGN_LEN);
    prefs.end();

    prefs.begin("id");
    prefs.remove(CONFIGMANAGER_KEY_UID);
    prefs.remove(CONFIGMANAGER_KEY_PICKLOCK);
    prefs.putBytes("cert_sign", factoryCertSign, BLELN_MANU_SIGN_LEN);
    prefs.end();

    prefs.begin("day");
    prefs.remove(CONFIGMANAGER_KEY_DLI);
    prefs.remove(CONFIGMANAGER_KEY_DS);
    prefs.remove(CONFIGMANAGER_KEY_DE);
    prefs.remove(CONFIGMANAGER_KEY_SSD);
    prefs.remove(CONFIGMANAGER_KEY_SRD);
    prefs.end();
}



