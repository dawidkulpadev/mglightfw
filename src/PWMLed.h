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

#ifndef UNTITLED_PWMLED_H
#define UNTITLED_PWMLED_H

#include <Arduino.h>
#include "driver/ledc.h"

#define PWMLED_V_INSTANT    0
#define PWMLED_MAX          1000

class PWMLed {
public:
    PWMLed(uint8_t ch, uint8_t pin, uint16_t freq, uint16_t fadingChangeTime=0);
    void start();
    void set(float val);

private:
    static bool fadingInstalled;
    uint8_t ch;
    uint8_t pin;
    uint16_t freq;
    uint16_t fcht; // Fading change time
};


#endif //UNTITLED_PWMLED_H
