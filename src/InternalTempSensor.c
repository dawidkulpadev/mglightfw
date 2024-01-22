//
// Created by dkulpa on 22.01.24.
//

#include "Arduino.h"
#include "driver/temp_sensor.h"
#include "InternalTempSensor.h"

float InternalTempSensor_read(){
    float result = NAN;
    temp_sensor_config_t tsens = TSENS_CONFIG_DEFAULT();
    temp_sensor_set_config(tsens);
    temp_sensor_start();
    delay(20);
    temp_sensor_read_celsius(&result);
    temp_sensor_stop();
    return result;
}