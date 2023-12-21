//
// Created by dkulpa on 21.12.23.
//

#ifndef MGLIGHTFW_CONSTS_H
#define MGLIGHTFW_CONSTS_H

/// Firmware version number = (hardware_version*10000)+(software_version*100)+(software_fix_version)
constexpr int hw_version=5;
constexpr int sw_version=5;
constexpr int sw_fix_version=2;

constexpr int fw_version= (hw_version*10000) + (sw_version*100) + sw_fix_version;

#endif //MGLIGHTFW_CONSTS_H
