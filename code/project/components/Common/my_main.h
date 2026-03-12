#ifndef MY_MAIN_H
#define MY_MAIN_H

#define MAX_NODE_NUM  32
#define MAC_LEN       6
#define INVALID_ID    0
#define ACK_TIMEOUT_MS     3000
#define ACK_RETRY_MAX      3


#define LORA_GETID_ADDRESS 0x12
#define CMD_DISCOVER_NODE   0x77
#define CMD_DISCOVER_NODE_RE  0x7A
#define CMD_ASSIGN_ID  0xA5
#define CMD_ASSIGN_ID_RE  0x5A
#define CMD_ID_ACK  0x66
#define CMD_ID_ACK_RE  0x6A
#define CMD_REQUEST 0x22  //重制
#define CMD_REQUEST_RE 0x2A
#define CMD_END 0xAA

#define CMD_CONTROL  0x21

#define CMD_FAN_ON  0x77
#define CMD_FAN_OFF 0x88

#define CMD_P_ON  0x99
#define CMD_P_OFF 0x96

#include <freeRTOS/FreeRTOS.h>

typedef struct {
    uint8_t mac[MAC_LEN];
    uint8_t node_id;
    bool    used;
    bool    online;
    uint8_t retry_cnt;
    TickType_t last_send_tick;
} node_entry_t;

bool gateway_delete_node_by_id(uint8_t node_id);

//事件定义
typedef enum {
    EVENT_NODE_ONLINE,
    EVENT_NODE_OFFLINE,
    EVENT_NODE_CONTROL,
} event_type_t;

#define TEMP_HIGH_TH   32.0
#define TEMP_LOW_TH    30.0
#define SOIL_DRY_TH    35.0
#define SOIL_OK_TH     40.0
#define EVT_TEMP_HIGH    BIT0
#define EVT_TEMP_NORMAL  BIT1

#define EVT_SOIL_DRY     BIT2
#define EVT_SOIL_OK      BIT3
#define EVT_TEMP_WILL_HIGH   BIT4   // 预测将要高温
#define EVT_SOIL_WILL_DRY    BIT5   // 预测将要干旱
#define EVT_ENV_WILL_BAD   BIT6
extern EventGroupHandle_t env_event_group;
#define PREDICT_WIN_SIZE   6   // 最近6次
#define SAMPLE_INTERVAL_S  5   // 5秒一次
#define PREDICT_TIME_S     300 // 预测5分钟后
#define TEMP_SLOPE_MIN   0.02f   // 每采样周期最小变化
#define SOIL_SLOPE_MIN   0.01f
#define THI_SLOPE_MIN  0.03f
typedef struct {
    float buf[PREDICT_WIN_SIZE];
    int   idx;
    int   count;
} linear_buf_t;
typedef enum {
    STATE_IDLE = 0,
    STATE_PREDICT_COOLING,   // 预测性通风
    STATE_COOLING,
    STATE_PREDICT_WATERING,  // 预测性灌溉
    STATE_WATERING,
} system_state_t;
typedef struct {
    float predict;
    float slope;     // 趋势强度
} predict_result_t;

typedef struct {
    const char *name;

    float temp_high;
    float temp_normal;

    float soil_dry;
    float soil_ok;

    float thi_high;
} crop_param_t;

typedef struct {
    uint16_t tempTh;
    uint16_t soilTh;
    uint16_t humiTh;
    uint16_t lightTh;
}ThVal_t;

typedef struct {
    bool fan_status;
    bool pump_status;
    bool led_status;
}FPStatus_t;

#endif