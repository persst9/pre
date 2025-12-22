#ifndef __MQTT_H__
#define __MQTT_H__

#include "esp_err.h"
typedef  struct node_t
{
    uint8_t node_id;
    int16_t temp;
    int16_t humi;
    int16_t light;
    int16_t soil;
}node_data_t;

#endif
