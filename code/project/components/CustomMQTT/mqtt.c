/**
 * @file mqtt_lora_onenet.c
 * @brief ESP32 对接OneNET MQTT平台 + LoRa数据收发示例
 * 功能：
 * 1. 连接WiFi网络，成功后启动MQTT客户端接入OneNET
 * 2. 订阅指定MQTT主题，接收云端下发指令
 * 3. 从LoRa模块读取传感器数据（温湿度、光照、土壤湿度）
 * 4. 将LoRa数据封装为JSON格式，发布到OneNET平台
 * 适配：ESP-IDF 5.x，基于官方MQTT组件 + 自定义LoRa驱动
 */

#include <stdio.h>
#include <string.h> // 补充字符串操作头文件（sprintf/snprintf依赖）
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_sntp.h"
#include <time.h>

#include "wifi_sta.h" // 自定义WiFi STA模式驱动（需自行实现）
// #include "cJSON.h"         // 可选：JSON解析库（注释掉表示直接拼接JSON）
#include "lora.h" // 自定义LoRa驱动（需自行实现）

// 日志标签：用于区分MQTT模块的日志输出
static const char *TAG = "MQTT";

extern const uint8_t emqxsl_ca_crt_start[] asm("_binary_emqxsl_ca_crt_start");
extern const uint8_t emqxsl_ca_crt_end[]   asm("_binary_emqxsl_ca_crt_end");
/**************************MQTT配置参数 **************************/
#define MQTT_ADDRESS "mqtts://qa1ba1e7.ala.cn-hangzhou.emqxsl.cn" // MQTT服务器地址
#define MQTT_PORT 8883                                            // MQTT端口（1883=TCP，8883=SSL/TLS）
#define MQTT_CLIENT   "ESP32_S3"                                 // 设备ID
#define MQTT_USERNAME "lin"                                       // （MQTT用户名）
// 密码
#define MQTT_PASSWORD "123456"

/************************** MQTT主题配置 **************************/
// 数据上报主题（JSON格式）
#define MQTT_PUBLIC_TOPIC "smart/pub"
#define MQTT_SUBSCRIBE_TOPIC "smart/sub" // 测试订阅主题（接收云端下发指令）

/************************** 全局事件与状态 **************************/
// 事件组：用于同步WiFi连接状态（通知main函数WiFi连接成功）
#define WIFI_CONNECT_BIT BIT0
static EventGroupHandle_t s_wifi_ev = NULL; // WiFi事件组句柄

/************************** 数据结构定义 **************************/
// LoRa节点传感器数据结构体
typedef struct node_t
{
    uint8_t node_id; // 节点ID（区分不同LoRa设备）
    int16_t temp;    // 温度（原始值，需除以10得到实际值，如255=25.5℃）
    int16_t humi;    // 湿度（原始值，如600=60.0%RH）
    int16_t light;   // 光照强度（原始值）
    int16_t soil;    // 土壤湿度（原始值）
} node_data_t;

/************************** 全局变量 **************************/
uint8_t data_buf[30] = {0};                           // LoRa接收数据缓冲区
char res_data[30] = {0};                              // 存储MQTT接收的消息内容
char res_topic[30] = {0};                             // 存储MQTT接收的消息主题
static esp_mqtt_client_handle_t s_mqtt_client = NULL; // MQTT客户端操作句柄
static bool s_is_mqtt_connected = false;              // MQTT连接状态标志
//时间同步
static void wait_for_time(void)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };

    int retry = 0;
    const int retry_count = 15;

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d)", retry);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "System time NOT set!");
    } else {
        ESP_LOGI(TAG, "System time is set");
    }
}

void sync_time(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_init();
}

/**
 * @brief MQTT事件处理回调函数
 *        MQTT客户端各类事件触发时调用，处理连接、断开、订阅、发布、收消息等逻辑
 * @param event_handler_arg 注册事件时传入的用户数据（此处为s_mqtt_client）
 * @param event_base 事件基类（固定为MQTT_EVENT_BASE）
 * @param event_id 事件ID（如连接成功、断开、收到消息等）
 * @param event_data 事件数据指针（esp_mqtt_event_handle_t类型）
 * @return 无
 */
static void aliot_mqtt_event_handler(void *event_handler_arg,
                                     esp_event_base_t event_base,
                                     int32_t event_id,
                                     void *event_data)
{
    // 类型转换：事件数据转为MQTT事件句柄
    esp_mqtt_event_handle_t event = event_data;
    // 获取MQTT客户端句柄
    esp_mqtt_client_handle_t client = event->client;

    // 根据事件ID分支处理
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED: // MQTT连接成功事件
        ESP_LOGI(TAG, "mqtt connected");
        s_is_mqtt_connected = true; // 更新连接状态为已连接
        // 连接成功后，订阅测试主题（QoS=1：至少一次送达）
        esp_mqtt_client_subscribe(s_mqtt_client, MQTT_SUBSCRIBE_TOPIC, 1);
        break;

    case MQTT_EVENT_DISCONNECTED: // MQTT连接断开事件
        ESP_LOGI(TAG, "mqtt disconnected");
        s_is_mqtt_connected = false; // 更新连接状态为断开
        break;

    case MQTT_EVENT_SUBSCRIBED: // 订阅主题成功（收到服务器ACK）
        ESP_LOGI(TAG, " mqtt subscribed ack, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED: // 取消订阅成功（收到服务器ACK）
        break;

    case MQTT_EVENT_PUBLISHED: // 发布消息成功（收到服务器ACK）
        ESP_LOGI(TAG, "mqtt publish ack, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: // 收到订阅主题的消息
        // 打印收到的主题和消息内容
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        // 将收到的主题/消息存储到全局变量（供其他函数使用）
        sprintf(res_topic, "%.*s", event->topic_len, event->topic);
        sprintf(res_data, "%.*s", event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR: // MQTT错误事件
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        ESP_LOGE(TAG, "last esp error: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGE(TAG, "tls stack error: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGE(TAG, "socket errno: %d", event->error_handle->esp_transport_sock_errno);
        break;

    default: // 其他未处理事件
        break;
    }
}

/**
 * @brief 初始化并启动MQTT客户端
 *        配置MQTT参数、初始化客户端、注册事件回调、启动连接
 * @param 无
 * @return 无
 */
void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.broker.address.uri = MQTT_ADDRESS;
    mqtt_cfg.broker.address.port = MQTT_PORT;
    //Client ID
    mqtt_cfg.credentials.client_id = MQTT_CLIENT;
    //用户名
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    //密码
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    mqtt_cfg.broker.verification.certificate =
    (const char *)emqxsl_ca_crt_start;

    mqtt_cfg.broker.verification.skip_cert_common_name_check = false;

    ESP_LOGI(TAG,"mqtt connect->clientId:%s,username:%s,password:%s",mqtt_cfg.credentials.client_id,
    mqtt_cfg.credentials.username,mqtt_cfg.credentials.authentication.password);
    //设置mqtt配置，返回mqtt操作句柄
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    //注册mqtt事件回调函数
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, aliot_mqtt_event_handler, s_mqtt_client);
    //启动mqtt连接
    esp_mqtt_client_start(s_mqtt_client);
}

/**
 * @brief WiFi事件回调函数（自定义WiFi驱动调用）
 *        当WiFi连接成功时，设置事件组标志位，通知MQTT启动
 * @param ev WiFi事件类型（此处仅处理WIFI_CONNECTED）
 * @return 无
 */
void wifi_event_handler(WIFI_EV_e ev)
{
    if (ev == WIFI_CONNECTED) // WiFi连接成功事件
    {
        // 设置事件组的WIFI_CONNECT_BIT标志位
        xEventGroupSetBits(s_wifi_ev, WIFI_CONNECT_BIT);
        ESP_LOGI(TAG, "WiFi connected, set WIFI_CONNECT_BIT");
    }
}

/**
 * @brief 读取LoRa传感器数据并发布到平台
 *        1. 从LoRa模块读取原始数据
 *        2. 解析为传感器数据结构体
 *        3. 封装为JSON格式，通过MQTT发布到
 * @param node_data 传感器数据结构体（输出参数，存储解析后的数据）
 * @return 无
 */
void pubData(node_data_t node_data)
{
    // 从LoRa模块接收数据到缓冲区（lora_receive需自行实现，阻塞/非阻塞需确认）
    lora_receive(data_buf, sizeof(data_buf));

    // 解析LoRa原始数据到结构体（需与LoRa发送端数据格式一致）
    node_data.node_id = data_buf[1]; // 第2字节：节点ID
    node_data.temp = data_buf[2];    // 第3字节：温度原始值
    node_data.humi = data_buf[3];    // 第4字节：湿度原始值
    node_data.light = data_buf[4];   // 第5字节：光照原始值
    node_data.soil = data_buf[5];    // 第6字节：土壤湿度原始值

    // 封装JSON格式消息（OneNET平台要求JSON格式）
    char mqtt_playload[128] = {0}; // JSON缓冲区（足够存储传感器数据）
    snprintf(mqtt_playload, sizeof(mqtt_playload),
             "{\"node_id\":%d,\"temp\":%.1f,\"humi\":%.1f,\"light\":%.1f,\"soil\":%.1f}",
             node_data.node_id,
             node_data.temp / 10.0, // 原始值/10 = 实际值（如25→2.5℃，需确认发送端倍率）
             node_data.humi / 10.0,
             node_data.light / 10.0,
             node_data.soil / 10.0);

    // 检查MQTT连接状态，避免断连时发布消息
    if (s_is_mqtt_connected)
    {
        // 发布消息到OneNET数据上报主题
        // 参数：客户端句柄、主题、消息内容、消息长度、QoS、是否保留
        esp_mqtt_client_publish(s_mqtt_client,
                                MQTT_PUBLIC_TOPIC,
                                mqtt_playload,
                                strlen(mqtt_playload),
                                0,  // QoS=0：最多一次送达（OneNET推荐QoS=0）
                                0); // retain=0：不保留消息
        ESP_LOGI(TAG, "Publish data to OneNET: %s", mqtt_playload);
    }
    else
    {
        ESP_LOGW(TAG, "MQTT not connected, skip publish");
    }
}

/**
 * @brief 处理MQTT订阅消息（预留函数）
 *        计划从res_data/res_topic中解析云端下发指令，控制LoRa节点
 * @param 无
 * @return node_data_t 解析后的指令数据（暂未实现）
 */
node_data_t subData(void)
{
    // 预留功能：解析MQTT接收的云端指令（如控制LoRa节点采集频率、校准传感器等）
    // 示例：if(strcmp(res_topic, MQTT_SUBSCRIBE_TOPIC) == 0) { 解析res_data }
    node_data_t empty_data = {0}; // 返回空数据（避免编译警告）
    return empty_data;
}

/**
 * @brief 应用入口函数
 *        初始化NVS、WiFi、事件组，等待WiFi连接后启动MQTT，循环运行
 * @param 无
 * @return 无
 */
void mqtt_task(void)
{
    // 初始化NVS闪存（WiFi配置、MQTT参数等非易失性存储）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS分区无空闲页或版本不匹配，擦除后重新初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 创建FreeRTOS事件组（用于同步WiFi和MQTT启动）
    s_wifi_ev = xEventGroupCreate();
    EventBits_t ev = 0;

    // 初始化WiFi STA模式（自定义驱动），传入WiFi事件回调函数
    wifi_sta_init(wifi_event_handler);

    // 阻塞等待WiFi连接成功（永久等待，直到WIFI_CONNECT_BIT被设置）
    // 参数：事件组、等待的标志位、清除标志位、等待所有位、最大等待时间
    ev = xEventGroupWaitBits(s_wifi_ev, WIFI_CONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    sync_time();
    wait_for_time();//等待时间同步完成
    if (ev & WIFI_CONNECT_BIT)       // WiFi连接成功
    {
        ESP_LOGI(TAG, "WiFi connected, start MQTT");
        mqtt_start(); // 启动MQTT客户端
    }

    // 临时缓冲区（未使用，可删除）
    static char mqtt_pub_buff[64];
    int count = 0;
    // 主循环：保持任务运行，可在此处添加数据采集/发布逻辑
    while (1)
    {
        
        //延时2秒发布一条消息到/test/topic1主题
        if(s_is_mqtt_connected)
        {
            snprintf(mqtt_pub_buff,64,"{\"count\":\"%d\"}",count);
            esp_mqtt_client_publish(s_mqtt_client, MQTT_PUBLIC_TOPIC,
                            mqtt_pub_buff, strlen(mqtt_pub_buff),1, 0);
            count++;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}