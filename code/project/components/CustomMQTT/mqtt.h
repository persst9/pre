#ifndef __MQTT_H__
#define __MQTT_H__
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
typedef struct {
    uint8_t node_id;
    char IdName[32];
    float temp;
    float humi;
    uint16_t light;
    uint16_t soil;
    bool fan;
    bool pump;
} node_data_t;

void mqtt_init(void);
void pubData(const char *data_buf);


// 定义要传递的数据包
typedef struct {
    char * keyThreshold; // 节点阈值
    uint16_t keyNum;
    uint8_t Id1;
    uint16_t valueThreshold;
    bool fan;  // 风扇状态（1=开启，0=关闭）
    bool pump; // 水泵状态（1=开启，0=关闭）
    bool light; // 灯光状态（1=开启，0=关闭）
    bool thresholdStatus; // 阈值状态（1=开启，0=关闭）
} nodeFP_t;

// 声明全局队列句柄
extern QueueHandle_t FanPQueue;
#include "freertos/event_groups.h"

extern EventGroupHandle_t gateway_event_group;

#define NEW_NODE_EVENT_BIT     (1 << 0)
#define DELETE_NODE_EVENT_BIT  (1 << 1)
#define NODE_DATA_EVENT_BIT    (1 << 2)

#endif
