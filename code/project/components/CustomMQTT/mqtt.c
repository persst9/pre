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
#include "mqtt.h"
#include "cJSON.h" // 可选：JSON解析库（注释掉表示直接拼接JSON）

#include "wifi_sta.h" // 自定义WiFi STA模式驱动
#include "lora.h"     // 自定义LoRa驱动
#include "my_main.h"
// 日志标签：用于区分MQTT模块的日志输出
static const char *TAG = "MQTT";

extern const uint8_t emqxsl_ca_crt_start[] asm("_binary_emqxsl_ca_crt_start");
extern const uint8_t emqxsl_ca_crt_end[] asm("_binary_emqxsl_ca_crt_end");
/**************************MQTT配置参数 **************************/
#define MQTT_ADDRESS "mqtts://qa1ba1e7.ala.cn-hangzhou.emqxsl.cn" // MQTT服务器地址
#define MQTT_PORT 8883                                            // MQTT端口（1883=TCP，8883=SSL/TLS）
#define MQTT_CLIENT "ESP32_S3"                                    // 设备ID
#define MQTT_USERNAME "lin"                                       // （MQTT用户名）
// 密码
#define MQTT_PASSWORD "123456"

/************************** MQTT主题配置 **************************/
// 数据上报主题（JSON格式）
#define MQTT_PUBLIC_TOPIC "/smart/sub"
#define MQTT_SUBSCRIBE_TOPIC "smart/pub" // 测试订阅主题（接收云端下发指令）
#define SMART_NODE_PREFIX "smartNode_"
/************************** 全局事件与状态 **************************/
// 事件组：用于同步WiFi连接状态（通知main函数WiFi连接成功）
#define WIFI_CONNECT_BIT BIT0
static EventGroupHandle_t s_wifi_ev = NULL; // WiFi事件组句柄

/************************** 全局变量 **************************/
static esp_mqtt_client_handle_t s_mqtt_client = NULL; // MQTT客户端操作句柄
static bool s_is_mqtt_connected = false;              // MQTT连接状态标志
void pubData(const char *data_buf);
EventGroupHandle_t gateway_event_group = NULL;
/**
 * @brief 解析 greenhouse 数据
 * @param json_str 接收到的原始 MQTT 字符串
 */
void parse_greenhouse_data(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "JSON 格式错误，无法解析");
        return;
    }
    cJSON *sendHeard = cJSON_GetObjectItem(root, "action");
    if (cJSON_IsString(sendHeard) && (sendHeard->valuestring != NULL))
    {
        if (sendHeard->valuestring != NULL && strcmp(sendHeard->valuestring, "addNode") == 0)
        {
            ESP_LOGI(TAG, "received addNode command");
            xEventGroupSetBits(gateway_event_group, NEW_NODE_EVENT_BIT); // 设置新节点事件标志
        }
        else if (sendHeard->valuestring != NULL && strcmp(sendHeard->valuestring, "deleteNode") == 0)
        {
            cJSON *nodeId = cJSON_GetObjectItem(root, "nodeId");
            if (cJSON_IsString(nodeId) && (nodeId->valuestring != NULL))
            {
                if (gateway_delete_node_by_id((uint8_t *)nodeId->valuestring))
                {
                    ESP_LOGI(TAG, "delete node success");
                }
            }
        }
        else if (sendHeard->valuestring != NULL && strcmp(sendHeard->valuestring, "Control") == 0)
        {
            nodeFP_t nodeData = {
                .Id1 = (uint8_t)cJSON_GetObjectItem(root, "id")->valueint,
                .fan = (uint8_t *)cJSON_GetObjectItem(root, "f")->valueint,
                .pump = (uint8_t *)cJSON_GetObjectItem(root, "p")->valueint,
                .light = (uint8_t *)cJSON_GetObjectItem(root, "L")->valueint,
                .thresholdStatus = false
            };
            xQueueSend(FanPQueue, &nodeData, portMAX_DELAY);
            xEventGroupSetBits(gateway_event_group, NODE_DATA_EVENT_BIT);
            ESP_LOGI(TAG, "receive Control");
        }
        else if (sendHeard->valuestring != NULL && strcmp(sendHeard->valuestring, "threshold") == 0)
        {
            ESP_LOGI(TAG, "receive threshold");
            nodeFP_t nodeDataT = {
                .Id1 = (uint8_t)cJSON_GetObjectItem(root, "nodeId")->valueint,
                .keyThreshold = (char *)cJSON_GetObjectItem(root, "key")->valuestring,
                .valueThreshold = (uint16_t)cJSON_GetObjectItem(root, "value")->valueint,
                .thresholdStatus = true
            };
            xQueueSend(FanPQueue, &nodeDataT, portMAX_DELAY);
        }
    }
    // 释放内存
    cJSON_Delete(root);
}

// 时间同步
static void wait_for_time(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};

    int retry = 0;
    const int retry_count = 15;

    while (timeinfo.tm_year < (2026 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d)", retry);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2026 - 1900))
    {
        ESP_LOGE(TAG, "System time NOT set!");
    }
    else
    {
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
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        char *received_data = malloc(event->data_len + 1);
        if (received_data)
        {
            memcpy(received_data, event->data, event->data_len);
            received_data[event->data_len] = '\0';

            // ESP_LOGI(TAG, "received data: %s", received_data);

            // 调用解析函数
            parse_greenhouse_data(received_data);

            free(received_data);
        }
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
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
    // Client ID
    mqtt_cfg.credentials.client_id = MQTT_CLIENT;
    // 用户名
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    // 密码
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    mqtt_cfg.broker.verification.certificate =
        (const char *)emqxsl_ca_crt_start;

    mqtt_cfg.broker.verification.skip_cert_common_name_check = false;

    ESP_LOGI(TAG, "mqtt connect->clientId:%s,username:%s,password:%s", mqtt_cfg.credentials.client_id,
             mqtt_cfg.credentials.username, mqtt_cfg.credentials.authentication.password);
    // 设置mqtt配置，返回mqtt操作句柄
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    // 注册mqtt事件回调函数
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, aliot_mqtt_event_handler, s_mqtt_client);
    // 启动mqtt连接
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

// 需确保NODE_ID宏定义正确（此处基于#define NODE_ID smartNode_调整，建议补充完整字符串）
// 若NODE_ID为节点名称常量，建议定义为：#define NODE_ID "smartNode_"
void pubData(const char *data_buf)
{
    if (s_is_mqtt_connected)
    {
        esp_mqtt_client_publish(s_mqtt_client,
                                MQTT_PUBLIC_TOPIC,
                                data_buf,
                                strlen(data_buf),
                                0,
                                0);
        ESP_LOGI(TAG, "Publish success: %s", data_buf);
    }
    else
    {
        ESP_LOGW(TAG, "MQTT not connected");
    }
}

// 补充说明：
// 1. 数据解析调整：原结构体为单字节数据，新结构体temp等为int16_t，需合并2字节（需与发送端字节序一致，默认大端）
//    若发送端为小端模式，需修改为：node_data.temp = (data_buf[3] << 8) | data_buf[2];
// 2. 节点名称：NODE_ID需定义为字符串（如#define NODE_ID "smartNode_01"），避免编译错误
// 3. 缓冲区扩大：从128字节改为256字节，适配节点名称字符串与更多字段内容
// 4. bool类型适配：JSON不直接支持bool，转为1/0数字，便于OneNET平台解析

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
void mqtt_init(void)
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
    wait_for_time();           // 等待时间同步完成
    if (ev & WIFI_CONNECT_BIT) // WiFi连接成功
    {
        ESP_LOGI(TAG, "WiFi connected, start MQTT");
        mqtt_start(); // 启动MQTT客户端
    }

    // // 临时缓冲区（未使用，可删除）
    // static char mqtt_pub_buff[64];
    // int count = 0;
    // // 主循环：保持任务运行，可在此处添加数据采集/发布逻辑
    // while (1)
    // {

    //     // 延时2秒发布一条消息到/test/topic1主题
    //     if (s_is_mqtt_connected)
    //     {
    //         snprintf(mqtt_pub_buff, 64, "{\"count\":\"%d\"}", count);
    //         esp_mqtt_client_publish(s_mqtt_client, MQTT_PUBLIC_TOPIC,
    //                                 mqtt_pub_buff, strlen(mqtt_pub_buff), 1, 0);
    //         count++;
    //     }

    //     vTaskDelay(pdMS_TO_TICKS(2000));
    // }
}