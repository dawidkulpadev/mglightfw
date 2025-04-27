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

#ifndef UNTITLED_DAY_H
#define UNTITLED_DAY_H

#include "Arduino.h"


class Day {
public:
    Day();

    float getSunIntensity(uint32_t dayTime, float lastIntensity);

    void setDli(int dli);
    void setDs(int ds);
    void setDe(int de);
    void setSsd(int ssd);
    void setSrd(int srd);

    int getDli();
    int getDs();
    int getDe();
    int getSsd();
    int getSrd();

private:
    int DLI=1000; //Daylight intensity (in min since 00:00)
    int DS;   //Day start (in min since 00:00)
    int DE;   //Day end (in min since 00:00)
    int SSD;  //Sunset duration (in min since 00:00)
    int SRD;  //Sunrise duration (in min since 00:00)
};


#endif //UNTITLED_DAY_H
