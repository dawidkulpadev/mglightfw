//
// Created by dkulpa on 21.12.23.
//

#ifndef MGLIGHTFW_CONSTS_H
#define MGLIGHTFW_CONSTS_H

const String api_url="https://dawidkulpa.pl/apis/miogiapicco/light";

/**
 * Firmware version number - 32 bit number
 * (16-bit)hw_id, (16-bit, 15-0 bits)sw_version
 * Hardware id: (6-bit) hw type, (10-bit) hw type version
 * Software version: (5-bit) sw epoch, (7-bit) sw epoch version, (4-bit) sw epoch version fix
 */

constexpr uint32_t hw_type= 1;
constexpr uint32_t hw_version= 5;
constexpr uint32_t hw_id= (hw_type<<10) | hw_version;

constexpr uint32_t sw_epoch= 3;
constexpr uint32_t sw_epoch_version= 1;
constexpr uint32_t sw_epoch_version_fix= 1;
constexpr uint32_t sw_version= (sw_epoch << 11) | (sw_epoch_version << 4) | sw_epoch_version_fix;

constexpr uint32_t fw_version= (hw_id << 16) | sw_version;

#endif //MGLIGHTFW_CONSTS_H
