//
// Created by dkulpa on 11.08.24.
//

#ifndef MGLIGHTFW_CONF_H
#define MGLIGHTFW_CONF_H

#define DEVICE_MODE_CONFIG             1
#define DEVICE_MODE_NORMAL             2

const std::string api_url="https://dawidkulpa.pl/apis/miogiapicco/";


/**
 * Firmware version number - 32 bit number
 * (16-bit)hw_id, (16-bit, 15-0 bits)sw_version
 * Hardware id: (6-bit) hw type, (10-bit) hw type version
 * Software version: (5-bit) sw epoch, (7-bit) sw epoch version, (4-bit) sw epoch version fix
 */

#define BLE_NAME    "MioGiapicco Light Gen2"
#define BLELN_CONFIG_UUID           "e0611e96-d399-4101-8507-1f23ee392891"
#define BLELN_HTTP_REQUESTER_UUID   "952cb13b-57fa-4885-a445-57d1f17328fd"

constexpr uint32_t sw_epoch= 3;
constexpr uint32_t sw_epoch_version= 1;
constexpr uint32_t sw_epoch_version_fix= 4;
constexpr uint32_t sw_version= (sw_epoch << 11) | (sw_epoch_version << 4) | sw_epoch_version_fix;


// 0001 1000 0001 0100 sw
// 0x1814

//#define HW_0_0
#define HW_1_5
//#define HW_1_6

#ifdef HW_0_0
constexpr uint32_t hw_type= 0;
constexpr uint32_t hw_version= 0;
constexpr uint32_t hw_id= (hw_type<<10) | hw_version;
// 0000 0100 0000 0101 hw
// 0x0405

constexpr uint32_t fw_version= (hw_id << 16) | sw_version;

constexpr uint8_t pinout_sys_led=      9;
constexpr uint8_t pinout_intensity=    6;
constexpr uint8_t pinout_switch=       4;
constexpr uint8_t pinout_fan=          5;
#endif

#ifdef HW_1_5

constexpr uint32_t hw_type= 1;
constexpr uint32_t hw_version= 5;
constexpr uint32_t hw_id= (hw_type<<10) | hw_version;
// 0000 0100 0000 0101 hw
// 0x0405

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
// 0000 0100 0000 0110 hw
// 0x0406

constexpr uint32_t fw_version= (hw_id << 16) | sw_version;

constexpr uint8_t pinout_sys_led=      7;
constexpr uint8_t pinout_intensity=    5;
constexpr uint8_t pinout_switch=       10;
constexpr uint8_t pinout_fan=          6;

#endif

#endif //MGLIGHTFW_CONF_H