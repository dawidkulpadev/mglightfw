//
// Created by dkulpa on 7.12.2025.
//

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
