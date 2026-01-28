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

#define DEVICE_CONFIG_ROLE_AUTO     '0'
#define DEVICE_CONFIG_ROLE_SERVER   '1'
#define DEVICE_CONFIG_ROLE_CLIENT   '2'


class DeviceConfig {
public:
    DeviceConfig();

    char *getSsid() const;
    char *getPsk() const;
    char *getUid() const;
    char *getPicklock() const;
    char *getTimezone() const;
    char getRole() const;

    void setSsid(const char *ssid);
    void setPsk(const char *psk);
    void setUid(const char *uid);
    void setPicklock(const char *picklock);
    void setTimezone(const char *timezone);
    void setRole(char role);

private:
    char *ssid;
    char *psk;
    char *uid;
    char *picklock;
    char *tz;
    char r;
};


#endif //UNTITLED_DEVICECONFIG_H
