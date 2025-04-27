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

#include "Day.h"

Day::Day() {
  
}

float Day::getSunIntensity(uint32_t dayTime, float lastIntensity) {
    double perc;
    int mins= dayTime/60;

    double lastPerc= (lastIntensity*10.0)/DLI;

    if(mins >= (DS+SRD) && mins<=(DE-SSD)){ // Full day
        perc= 1.0;
    } else if(mins>DS && mins<(DS+SRD)){ // Sunrise
        double a= 1.0/(double)SRD;
        double b= -a*(double)DS;

        perc= (a*(double)mins)+b;

        if(perc<lastPerc){
            perc= lastPerc;
        }
    } else if(mins>(DE-SSD) && mins<DE) { // Sunset
        double a= -1.0/(double)SSD;
        double b= -a*(double)DE;

        perc= (a*(double)mins)+b;

        if(perc>lastPerc){
            perc= lastPerc;
        }
    } else { // Night
        perc= 0.0;
    }

    //intensityValid= true;
    return perc*(double)DLI/10.0;
}

void Day::setDli(int dli) {
    DLI = dli;
}

void Day::setDs(int ds) {
    DS = ds;
}

void Day::setDe(int de) {
    DE = de;
}

void Day::setSsd(int ssd) {
    SSD = ssd;
}

void Day::setSrd(int srd) {
    SRD = srd;
}

int Day::getDli(){return DLI;}
int Day::getDs(){return DS;}
int Day::getDe(){return DE;}
int Day::getSsd(){return SSD;}
int Day::getSrd(){return SRD;}
