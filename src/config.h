//
// Created by dkulpa on 11.08.24.
//

#ifndef MGLIGHTFW_CONF_H
#define MGLIGHTFW_CONF_H

const String api_url="https://dawidkulpa.pl/apis/miogiapicco/light";

/**
 * Firmware version number - 32 bit number
 * (16-bit)hw_id, (16-bit, 15-0 bits)sw_version
 * Hardware id: (6-bit) hw type, (10-bit) hw type version
 * Software version: (5-bit) sw epoch, (7-bit) sw epoch version, (4-bit) sw epoch version fix
 */

//#define HW_1_5
#define HW_1_6


#ifdef HW_1_5

constexpr uint32_t hw_type= 1;
constexpr uint32_t hw_version= 5;
constexpr uint32_t hw_id= (hw_type<<10) | hw_version;

constexpr uint32_t sw_epoch= 3;
constexpr uint32_t sw_epoch_version= 1;
constexpr uint32_t sw_epoch_version_fix= 1;
constexpr uint32_t sw_version= (sw_epoch << 11) | (sw_epoch_version << 4) | sw_epoch_version_fix;

constexpr uint32_t fw_version= (hw_id << 16) | sw_version;

constexpr uint8_t pinout_sys_led=      5;
constexpr uint8_t pinout_intensity=    4;
constexpr uint8_t pinout_switch=       10;
constexpr uint8_t pinout_fan=          0;

#endif


#ifdef HW_1_6

constexpr uint32_t hw_type= 1;
constexpr uint32_t hw_version= 6;
constexpr uint32_t hw_id= (hw_type<<10) | hw_version;

constexpr uint32_t sw_epoch= 3;
constexpr uint32_t sw_epoch_version= 1;
constexpr uint32_t sw_epoch_version_fix= 2;
constexpr uint32_t sw_version= (sw_epoch << 11) | (sw_epoch_version << 4) | sw_epoch_version_fix;

constexpr uint32_t fw_version= (hw_id << 16) | sw_version;

constexpr uint8_t pinout_sys_led=      7;
constexpr uint8_t pinout_intensity=    5;
constexpr uint8_t pinout_switch=       10;
constexpr uint8_t pinout_fan=          6;

#endif

#endif //MGLIGHTFW_CONF_H