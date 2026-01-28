/**
    MioGiapicco Light Firmware - Firmware for Light Device of MioGiapicco system
    Copyright (C) 2026  Dawid Kulpa

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

#include "BLELNBase.h"

const char* BLELNBase::SERVICE_UUID   = "952cb13b-57fa-4885-a445-57d1f17328fd";
const char* BLELNBase::KEY_TO_CLI_UUID  = "ef7cb0fc-53a4-4062-bb0e-25443e3a1f5d";
const char* BLELNBase::KEY_TO_SER_UUID  = "345ac506-c96e-45c6-a418-56a2ef2d6072";
const char* BLELNBase::DATA_TO_CLI_UUID   = "b675ddff-679e-458d-9960-939d8bb03572";
const char* BLELNBase::DATA_TO_SER_UUID   = "566f9eb0-a95e-4c18-bc45-79bd396389af";

void BLELNBase::bytes_to_hex(const uint8_t *src, size_t src_len) {
    const char hex_map[] = "0123456789ABCDEF";
    char buf[(src_len * 2) + 1];

    for (size_t i = 0; i < src_len; i++) {
        uint8_t byte = src[i];

        buf[i * 2] = hex_map[(byte >> 4) & 0x0F];
        buf[i * 2 + 1] = hex_map[byte & 0x0F];
    }
    buf[src_len * 2] = '\0';

    Serial.printf("%s\r\n", buf);
}
