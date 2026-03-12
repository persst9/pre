#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "driver/gpio.h"
#include <esp_log.h>
#include <freertos/queue.h>
#include "driver/uart.h" // ESP32的UART驱动头文件，包含串口相关函数和宏
#include "cJSON.h"
#include "esp_system.h"
#include "esp_mac.h"

#include "my_nvs.h"
#include "my_main.h"
#include "bh1750.h"
#include "dht11.h"
#include "ware.h"
#include "TS.h"
#include "mqtt.h"
#include "lora.h"

#define SERVER 1
#define LORA_NUM 1
#define LORA_MAX_FRAME 128
static const char *TAG = "main";
TaskHandle_t add_node_task_handle = NULL;
static uint8_t uart_buffer[256]; // 定义UART接收缓冲区
static char frame_buf[128];
static int frame_len = 0;
QueueHandle_t FanPQueue = NULL;               // 传感器数据队列
node_data_t node_data[LORA_NUM];              // 节点数据
static node_entry_t node_table[MAX_NODE_NUM]; // 节点表
uint8_t node_id = 0;                          // 节点ID
char nodeIdName[10] = "node_";                // 节点ID字符串
static uint8_t nodeNum = 0;                   // 节点数量
static bool nodeAddStatus = false;            // 节点添加状态
uint8_t macId[6];                             // 节点MAC地址
static ThVal_t thVal = {0};                   // 温湿度值
static FPStatus_t fpStatus = {0};             // 风扇、水泵、补光灯状态
static const crop_param_t crop_table[] = {
    {
        .name = "Tomato",
        .temp_high = 32,
        .temp_normal = 30,
        .soil_dry = 35,
        .soil_ok = 45,
        .thi_high = 60,
    },
    {
        .name = "Lettuce",
        .temp_high = 26,
        .temp_normal = 24,
        .soil_dry = 40,
        .soil_ok = 50,
        .thi_high = 52,
    }};
static const crop_param_t *crop = &crop_table[0];

static void fan_on_high(void) { ESP_LOGI(TAG, "Fan ON"); }
static void fan_off(void) { ESP_LOGI(TAG, "Fan OFF"); }
static void fan_on_low(void) { ESP_LOGI(TAG, "Fan ON LOW"); }
static void pump_on(void) { ESP_LOGI(TAG, "Pump ON"); }
static void pump_off(void) { ESP_LOGI(TAG, "Pump OFF"); }
static void pump_on_pulse(void) { ESP_LOGI(TAG, "Pump ON LOW"); }
static void light_on(void) { ESP_LOGI(TAG, "Light ON"); }
static void light_off(void) { ESP_LOGI(TAG, "Light OFF"); }
typedef struct
{
    uint16_t len;
    uint8_t data[LORA_MAX_FRAME];
} lora_frame_t;
QueueHandle_t lora_frame_queue;
EventGroupHandle_t env_event_group;
/***************************Public********************************* */
/*
 * @brief: 读取节点ID
 * @Server:读取已经添加的节点ID
 * @Client:读取本机ID
 */
void nvsReadNodeIdInit(void)
{
#if SERVER
    nodeNum = read_nvs_u8(NVS_NODEID_NAMESPACE, NVS_NODEID_NUM);
    if (nodeNum == 0)
    {
        ESP_LOGI(TAG, "nodeNum is 0");
        return;
    }
    else
    {
        ESP_LOGI(TAG, "nodeNum is %d", nodeNum);
    }
    for (int i = 0; i < nodeNum; i++)
    {
        char nid[8] = {0}; // 节点ID字符串
        if (readNodesId(i + 1, nid, sizeof(nid)) == 0)
        {
            ESP_LOGI(TAG, "readNodesId error %u", i + 1);
            continue;
        }
        node_table[i].node_id = i + 1;
        memcpy(node_table[i].mac, nid, 6);
        node_table[i].used = true;
    }
#else
    node_id = read_nvs_u8(NVS_NODEID_NAMESPACE, NVS_NODEID_KEY);
    ESP_LOGI(TAG, "Node ID: %d", node_id);
    snprintf(nodeIdName, sizeof(nodeIdName), "%d", node_id);
#endif
}

/****************************************************************** */

/***************************Server***************************** */
/**
 * @brief 根据MAC地址查找节点在节点表中的索引
 * @param mac 待查找的节点MAC地址指针（长度固定为6字节）
 * @return 找到则返回节点表索引（0~MAX_NODE_NUM-1），未找到返回-1
 */
int find_node_by_mac(const uint8_t *mac)
{
    // 遍历整个节点表
    for (int i = 0; i < MAX_NODE_NUM; i++)
    {
        // 检查节点是否被占用，且MAC地址完全匹配
        if (node_table[i].used &&
            memcmp(node_table[i].mac, mac, 6) == 0) // memcmp返回0表示6字节MAC地址完全相同
        {
            return i; // 找到匹配节点，返回索引
        }
    }
    return -1; // 未找到匹配节点
}
/**
 * @brief 为新节点分配节点ID（从节点表中找第一个未使用的位置）
 * @param mac 待分配ID的节点MAC地址指针（长度固定为6字节）
 * @return 分配成功返回节点ID（1~MAX_NODE_NUM），节点表满返回0
 */
uint8_t alloc_node_id(const uint8_t *mac)
{
    esp_err_t ret = ESP_OK;
    // 遍历节点表，寻找第一个未使用的位置
    for (int i = 0; i < MAX_NODE_NUM; i++)
    {
        if (!node_table[i].used) // 找到未使用的节点位置
        {
            node_table[i].used = true;                              // 标记为已使用
            memcpy(node_table[i].mac, mac, 6);                      // 保存节点MAC地址
            node_table[i].node_id = i + 1;                          // 分配节点ID（ID从1开始，避免0作为有效ID）
            ret = writeNodesId(node_table[i].node_id, (char *)mac); // 将分配的ID写入节点表
            if (ret != ESP_OK)
            {
                printf("writeNodesId failed\n");
                return 0; // 写入节点表失败，分配失败
            }
            else
            {
                printf("writeNodesId success\n");
            }
            return node_table[i].node_id; // 返回分配的ID
        }
    }
    return 0; // 节点表已满，分配失败
}

/**
 * @brief 处理网关收到的节点获取ID请求
 * @param data 接收到的LoRa数据缓冲区指针
 * @param len  接收到的数据长度
 * @note 数据格式要求：[0] = LORA_GETID_ADDRESS, [1~6] = MAC地址，总长度必须为7字节
 */
void gateway_handle_getid(uint8_t *data, size_t len)
{
    // 校验数据长度是否合法（必须为7字节：1字节地址 + 6字节MAC）
    if (len != 8)
        return;
    // 校验地址标识是否匹配
    if (data[0] != LORA_GETID_ADDRESS)
        return;

    // 提取MAC地址（数据第2~7字节）
    uint8_t *mac = &data[1];

    // 打印日志，格式化输出MAC地址
    ESP_LOGI(TAG,
             "Node found MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 根据MAC查找节点是否已分配ID
    int idx = find_node_by_mac(mac);
    uint8_t node_id;

    if (idx >= 0)
    {
        // 节点已存在，复用原有ID
        node_id = node_table[idx].node_id;
        ESP_LOGI(TAG, "Reuse node_id=%d", node_id);
        return; // 直接返回，不再继续处理
    }
    else
    {
        // 节点不存在，分配新ID
        node_id = alloc_node_id(mac);
        ESP_LOGI(TAG, "Assign new node_id=%d", node_id);
    }

    // 构造回复数据：[0] = 指令码, [1~6] = MAC地址, [7] = 节点ID
    uint8_t sendData[9];
    sendData[0] = CMD_ASSIGN_ID;           // 设置分配ID指令码
    memcpy(&sendData[1], mac, 6);          // 填充节点MAC地址
    sendData[7] = node_id;                 // 填充分配的节点ID
    node_table[node_id - 1].online = true; // 标记节点在线false;
    node_table[node_id - 1].retry_cnt = 0;
    node_table[node_id - 1].last_send_tick = xTaskGetTickCount();
    sendData[8] = CMD_END; // 填充结束标志
    // nodeAddStatus = true;
    // 通过LoRa发送回复数据给节点
    lora_send(sendData, sizeof(sendData));
}

void gateway_add_node_start(void)
{
    // ESP_LOGI(TAG, "Gateway enter add-node mode");

    // // 清空在线状态（可选）
    // for (int i = 0; i < MAX_NODE_NUM; i++) {
    //     node_table[i].online = false;
    //     node_table[i].retry_cnt = 0;
    // }
    static uint8_t send_Count = 0;
    // 广播寻找节点
    uint8_t cmd[8] = {
        CMD_DISCOVER_NODE,
        0, 1, 2, 3, 4, 5, CMD_END};
    if (send_Count < 10)
    {
        lora_send(&cmd, 8);
        ESP_LOGI(TAG, "Gateway broadcast CMD_DISCOVER_NODE");
        send_Count++;
    }
}

/**
 * @brief 处理网关收到的节点ID确认应答
 * @param data 接收到的LoRa数据缓冲区指针
 * @param len  接收到的数据长度
 * @note 数据格式要求：[0] = CMD_ID_ACK, [1~6] = MAC地址, [7] = 节点ID，总长度必须为8字节
 */
void gateway_handle_id_ack(uint8_t *data, size_t len)
{
    // 校验数据长度是否合法（必须为8字节：1字节指令 + 6字节MAC + 1字节ID）
    if (len != 8 || data[0] != CMD_ID_ACK)
        return;
    // 提取MAC地址和节点ID
    uint8_t *mac = &data[1];
    uint8_t node_id = data[7];

    // 根据MAC查找节点是否已分配ID
    int idx = find_node_by_mac(mac);
    if (idx < 0)
    {
        // 未找到该MAC对应的节点，打印警告日志
        ESP_LOGW(TAG, "ACK from unknown MAC");
        return;
    }

    // 校验应答的节点ID与本地记录是否一致
    if (node_table[idx].node_id != node_id)
    {
        ESP_LOGW(TAG, "ACK node_id mismatch");
        return;
    }

    // 标记节点为在线状态
    node_table[idx].online = true;
    node_table[idx].retry_cnt = 0; // 重置重试计数器
    // 打印日志，确认节点应答成功
    ESP_LOGI(TAG,
             "Node ACK OK: ID=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             node_id,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief ACK超时重传任务（FreeRTOS任务）
 * @param pvParameters 任务参数（此处未使用）
 * @note 任务功能：
 *       1. 周期性检查所有已分配ID但未上线的节点
 *       2. 若超过ACK超时时间未收到应答，则重传ID分配指令
 *       3. 重传次数达到最大值后停止重试
 *       4. 任务周期为500ms，降低CPU占用
 */
void ack_timeout_task(void *pvParameters)
{
    // 任务主循环（无限循环，常驻运行）
    while (1)
    {
        // 获取当前系统tick值（FreeRTOS系统节拍，用于时间计算）
        TickType_t now = xTaskGetTickCount();

        // 遍历所有节点，检查需要重传的节点
        for (int i = 0; i < MAX_NODE_NUM; i++)
        {
            // 跳过条件：节点未被占用 或 节点已在线
            if (!node_table[i].used || node_table[i].online)
                continue;

            // 检查是否达到最大重传次数，达到则停止重试
            if (node_table[i].retry_cnt >= ACK_RETRY_MAX)
            {
                ESP_LOGW(TAG, "Node %d ACK timeout, stop retry",
                         node_table[i].node_id);
                continue;
            }

            // 计算从最后一次发送到现在的时间差（转换为毫秒）
            // portTICK_PERIOD_MS：每个tick对应的毫秒数（FreeRTOS配置）
            if ((now - node_table[i].last_send_tick) * portTICK_PERIOD_MS >= ACK_TIMEOUT_MS)
            {

                // 打印重传日志，提示当前重传次数
                ESP_LOGW(TAG, "Resend ID to node %d (retry %d)",
                         node_table[i].node_id,
                         node_table[i].retry_cnt + 1);

                // 构造重传的ID分配指令数据
                uint8_t sendData[8];
                sendData[0] = CMD_ASSIGN_ID;                // 指令码：分配ID
                memcpy(&sendData[1], node_table[i].mac, 6); // 填充节点MAC地址
                sendData[7] = node_table[i].node_id;        // 填充节点ID

                // 通过LoRa发送重传数据
                lora_send(sendData, sizeof(sendData));

                // 更新重传计数和最后发送时间
                node_table[i].retry_cnt++;          // 重传次数+1
                node_table[i].last_send_tick = now; // 记录本次发送的tick值
            }
        }

        // 任务挂起500ms，降低轮询频率，减少系统资源占用
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void loraDataDeal(const char *data_buf, int len)
{
}

static void ServerDataClient(void *arg)
{
    lora_frame_t frame; // 解析数据
    while (1)
    {
        if (xQueueReceive(lora_frame_queue, &frame, portMAX_DELAY))
        {
            if (frame.data[0] != '{')
            {
                switch (frame.data[0])
                {
                case LORA_GETID_ADDRESS: // 获取节点ID
                    gateway_handle_getid(frame.data, frame.len);
                    break;
                case CMD_ID_ACK: //
                    gateway_handle_id_ack(frame.data, frame.len);
                    break;
                case CMD_ASSIGN_ID_RE:    // 分配节点ID成功
                    nodeAddStatus = true; // 添加节点成功;
                    break;
                case 0x5f:
                    erase_nvs_namespace(NVS_NODEID_NAMESPACE);
                    break;
                default:
                    break;
                }
            }
            else
            {
                printf("lora data:%s\n", frame.data);
                pubData((char *)frame.data);
            }
        }
    }
}
/*
 * @brief: LoRa MQTT任务
 * @param: void *pvParameters
 * @return: void
 * @note: LoRa MQTT任务，用于处理接收到的数据，并将其发送到对应的lora节点
 */
void mqttSendDataLoraTask(void *pvParameters)
{
    nodeFP_t fpStatus = {0};
    while (1)
    {
        if (xQueueReceive(FanPQueue, &fpStatus, portMAX_DELAY) == pdPASS)
        {
            char buf[128]; // 确保缓冲区足够大
                           // 格式字符串中使用 %d，参数直接传 bool/int
            if (fpStatus.thresholdStatus == true)
            {
                snprintf(buf, sizeof(buf),
                         "{\"node_id\":node_\"%d\",\"Key\":%s,\"val\":%d,\"action\":%s}",
                         fpStatus.Id1,
                         (char *)fpStatus.keyThreshold,
                         fpStatus.valueThreshold,
                         "threshold");
            }
            else
            {
                snprintf(buf, sizeof(buf),
                         "{\"node_id\":node_\"%d\",\"fan\":%d,\"pump\":%d,\"LED\":%d,\"action\":%s}",
                         fpStatus.Id1,
                         fpStatus.fan ? 1 : 0, // 转为 1 或 0
                         fpStatus.pump ? 1 : 0,
                         fpStatus.light ? 1 : 0,
                         "control");
            }

            lora_send((uint8_t *)buf, strlen(buf));
            ESP_LOGI("LORA", "已发送: %s", buf);
        }
    }
}
void add_node_task(void *pvParameters)
{
    while (1)
    {
        gateway_add_node_start();
        // 等待一段时间（给节点分配 ID + ACK）
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (nodeAddStatus == true && nodeNum + 1 < MAX_NODE_NUM)
        {
            nodeNum++;
            write_nvs_u8(NVS_NODEID_NAMESPACE, NVS_NODEID_NUM, nodeNum);
            ESP_LOGI(TAG, "Add node task finished, suspend, nodeNum=%u", nodeNum);
            nodeAddStatus = false;
            vTaskSuspend(NULL); // 挂起自己
        }
    }
}
/*
网关分配id任务
*/
void sendNodeIDTask(void *pvParameters)
{
    static uint8_t nodeIdCount = 0;
    EventGroupHandle_t event_group = (EventGroupHandle_t)pvParameters; // 获取事件组句柄
    // 关键检查：确保事件组句柄有效
    if (event_group == NULL)
    {
        ESP_LOGE(TAG, "事件组句柄为空，任务退出！");
        vTaskDelete(NULL); // 删除当前任务，避免崩溃
        return;
    }
    EventBits_t uxBits;
    while (1)
    {
        // 等待事件（自动阻塞）
        uxBits = xEventGroupWaitBits(
            event_group,
            NEW_NODE_EVENT_BIT,
            pdTRUE, // 自动清除bit
            pdFALSE,
            portMAX_DELAY);
        // 判断事件是否真的触发
        if (uxBits & NEW_NODE_EVENT_BIT)
        {
            // 事件触发，执行ID分配逻辑
            printf("Start assigning node ID...\n");
            printf("Done. Waiting for next...\n");
            if (add_node_task_handle)
            {
                vTaskResume(add_node_task_handle);
                ESP_LOGI(TAG, "Node task resumed");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
bool gateway_delete_node_by_id(uint8_t node_id)
{
    for (int i = 0; i < MAX_NODE_NUM; i++)
    {
        if (node_table[i].used &&
            node_table[i].node_id == node_id)
        {
            ESP_LOGI(TAG, "Delete node ID=%d", node_id);
            // // 可选：通知节点
            uint8_t sendData[8];
            sendData[0] = CMD_REQUEST;
            memcpy(&sendData[1], node_table[i].mac, 6);
            sendData[7] = CMD_END;
            lora_send(sendData, sizeof(sendData));
            erase_nvs_id(node_id);
            // 清空表项（回收 ID）
            memset(&node_table[i], 0, sizeof(node_entry_t));
            return true;
        }
        else
        {
            ESP_LOGI(TAG, "Node ID=%d not found", node_id);
        }
    }
    return false;
}

/*************************Server code end************************ */

/*************************Node*********************************** */
/**
 * @brief  网关解析节点上报的JSON数据
 */
void loraDataSendServer(const char *data)
{
    lora_send((uint8_t *)data, strlen(data));
    ESP_LOGI(TAG, "send data:%s", data);
}

static void loraDataClient(void *arg)
{
    lora_frame_t frame; // 解析数据
    while (1)
    {
        if (xQueueReceive(lora_frame_queue, &frame, portMAX_DELAY))
        {
            ESP_LOGI("loraDataClient", "receive data[0]:%u", frame.data[0]);
            if (frame.data[0] != '{')
            {
                switch (frame.data[0])
                {
                case CMD_ASSIGN_ID: // 分配 ID 0xa5
                    ESP_LOGI(TAG, "CMD_ASSIGN_ID");
                    bool isAssignStatus = true;
                    for (uint8_t i = 0; i < 6; i++)
                    {
                        if (macId[i] != frame.data[i + 1])
                        {
                            ESP_LOGW(TAG, "MAC not match");
                            isAssignStatus = false;
                            break;
                        }
                    }
                    if (isAssignStatus)
                    {
                        node_id = frame.data[7]; // 正确位置
                        ESP_LOGI(TAG, "Assigned node_id = %d", node_id);
                        uint8_t ack[9];
                        ack[0] = CMD_ASSIGN_ID_RE;
                        memcpy(&ack[1], macId, 6);
                        ack[7] = node_id;
                        ack[8] = CMD_END;
                        // 存储节点ID
                        write_nvs_u8(NVS_NODEID_NAMESPACE, NVS_NODEID_KEY, node_id);
                        lora_send(ack, sizeof(ack));
                    }
                    break;

                case CMD_DISCOVER_NODE: // 请求 ID  0xaa
                {
                    ESP_LOGI(TAG, "CMD_DISCOVER_NODE");
                    if (node_id == 0)
                    {
                        uint8_t sendData[8] = {
                            LORA_GETID_ADDRESS, // 0x12
                            macId[0], macId[1], macId[2],
                            macId[3], macId[4], macId[5], CMD_END};
                        lora_send(sendData, sizeof(sendData));
                    }
                    break;
                }

                case 0x99: // 心跳 / 校验
                {
                    uint8_t ack_Status = 0;
                    ESP_LOGI(TAG, "CMD_HEARTBEAT");
                    for (uint8_t i = 0; i < 6; i++)
                    {
                        if (macId[i] != frame.data[i + 1])
                        {
                            ack_Status = 1;
                            break;
                        }
                    }
                    if (ack_Status == 0)
                    {
                        uint8_t sendData[8] = {
                            CMD_ID_ACK, // 0x66
                            macId[0], macId[1], macId[2],
                            macId[3], macId[4], macId[5], CMD_END};
                        lora_send(sendData, sizeof(sendData));
                    }
                    break;
                }

                case CMD_REQUEST: // 0x22
                {
                    ESP_LOGI(TAG, "CMD_REQUEST");
                    bool isExist = true;
                    for (uint8_t i = 0; i < 6; i++)
                    {
                        if (macId[i] != frame.data[i + 1])
                        {
                            isExist = false;
                            ESP_LOGI(TAG, "not exist, macId[%d] = %02x, frame.data[%d] = %02x", i, macId[i], i + 1, frame.data[i + 1]);
                            break;
                        }
                    }
                    if (isExist == true)
                    {
                        node_id = 0;
                        write_nvs_u8(NVS_NODEID_NAMESPACE, NVS_NODEID_KEY, node_id);
                        uint8_t sendData[8] = {
                            CMD_REQUEST_RE, // 0x66
                            macId[0], macId[1], macId[2],
                            macId[3], macId[4], macId[5], CMD_END};
                        lora_send(sendData, sizeof(sendData));
                        ESP_LOGI(TAG, "NODE ID :%d", node_id);
                    }
                    break;
                }
                default:
                    ESP_LOGI(TAG, "CMD: %d", frame.data[0]);
                    break;
                }
            }
            else if (frame.data[0] == '{')
            {
                ESP_LOGI(TAG, "JSON: %s", frame.data);
                cJSON *root = cJSON_Parse((const char *)frame.data);
                if (root == NULL)
                {
                    ESP_LOGE(TAG, "cJSON_Parse error");
                    continue;
                }
                cJSON *nodeid0 = cJSON_GetObjectItem(root, "node_id"); // 格式
                if (nodeid0 == NULL)
                {
                    ESP_LOGE(TAG, "cJSON_GetObjectItem error");
                    continue;
                }
                else if (nodeid0->valuestring != NULL)
                {
                    // 比较节点ID字符串是否一致
                    if (strcmp(nodeid0->valuestring, nodeIdName) == 0)
                    {
                        ESP_LOGI(TAG, "nodeIdName:%s", nodeIdName);
                        if (strcmp(cJSON_GetObjectItem(root, "action")->valuestring, "threshold") == 0)
                        {
                            char *KeyTh = cJSON_GetObjectItem(root, "Key")->valuestring;
                            uint16_t ValueTh = cJSON_GetObjectItem(root, "val")->valueint;
                            if (strcmp(KeyTh, "temp") == 0)
                            {
                                thVal.tempTh = ValueTh;
                                write_Data_Threshold(NODE_TEMP_KEY, thVal.tempTh);
                            }
                            else if (strcmp(KeyTh, "humi") == 0)
                            {
                                thVal.humiTh = ValueTh;
                                write_Data_Threshold(NODE_HUMI_KEY, thVal.humiTh);
                            }
                            else if (strcmp(KeyTh, "light") == 0)
                            {
                                thVal.lightTh = ValueTh;
                                write_Data_Threshold(NODE_LIGHT_KEY, thVal.lightTh);
                            }
                            else if (strcmp(KeyTh, "soil") == 0)
                            {
                                thVal.soilTh = ValueTh;
                                write_Data_Threshold(NODE_SOIL_KEY, thVal.soilTh);
                            }
                        }
                        else if (strcmp(cJSON_GetObjectItem(root, "action")->valuestring, "control") == 0)
                        {
                            cJSON *fanStatus = cJSON_GetObjectItem(root, "fan");
                            if (fanStatus == NULL)
                            {
                                ESP_LOGE(TAG, "fan error");
                            }
                            else
                            {
                                if (fanStatus->valueint == 1)
                                {
                                    // FAN_ON;
                                    fan_on_high();
                                }
                                else
                                {
                                    // FAN_OFF;
                                    fan_off();
                                }
                            }
                            cJSON *pumpStatus = cJSON_GetObjectItem(root, "pump");
                            if (pumpStatus == NULL)
                            {
                                ESP_LOGE(TAG, "pump error");
                            }
                            else
                            {
                                if (pumpStatus->valueint == 1)
                                {
                                    pump_on();
                                    // PUMP_ON;
                                }
                                else
                                {
                                    // PUMP_OFF;
                                    pump_off();
                                }
                            }
                            cJSON *lightStatus = cJSON_GetObjectItem(root, "light");
                            if (lightStatus == NULL)
                            {
                                ESP_LOGE(TAG, "light error");
                            }
                            else
                            {
                                if (lightStatus->valueint == 1)
                                {
                                    light_on();
                                }
                                else
                                {
                                    light_off();
                                }
                            }
                        }
                    }
                }
                cJSON_Delete(root);
            }
        }
    }
}

void lora_task(void *pvParameters)
{
    uart_event_t event; // 定义串口事件结构体，用于存储接收到的事件
    // 无限循环，持续监听串口事件
    while (1)
    {
        // 从串口事件队列中接收事件，portMAX_DELAY表示永久阻塞等待，直到有事件到来
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            // 打印事件所属的串口编号，方便调试
            // ESP_LOGI(TAG, "uart[%d] event:", USER_UART_NUM);

            // 根据事件类型进行分支处理
            switch (event.type)
            {
            // 事件类型：接收到数据（最核心的事件）
            case UART_DATA:
                // 打印接收到的数据长度
                ESP_LOGI(TAG, "[UART DATA LEN]: %d", event.size);
                // 从串口读取指定长度的数据到缓冲区，portMAX_DELAY表示阻塞等待直到读取完成
                int len = uart_read_bytes(USER_UART_NUM,
                                          uart_buffer,
                                          event.size,
                                          portMAX_DELAY);
                if (len <= 0)
                {
                    ESP_LOGE(TAG, "UART read failed");
                    break;
                }
                // 打印接收到的数据
                ESP_LOGI(TAG, "[UART DATA[0]]: %u", uart_buffer[0]);
                ESP_LOG_BUFFER_HEX(TAG, uart_buffer, len);
                for (int i = 0; i < len; i++)
                {
                    uint8_t c = uart_buffer[i];
                    if (frame_len >= LORA_MAX_FRAME)
                    {
                        frame_len = 0;
                        break;
                    }
                    frame_buf[frame_len++] = c;
                    if (c == '}' || (c == CMD_END && frame_len >= 8 && frame_len <= 10))
                    {
                        if (frame_len > 0)
                        {
                            lora_frame_t frame = {0};
                            frame.len = frame_len;
                            memcpy(frame.data, frame_buf, frame_len);
                            // 打印完整帧（方便调试）
                            ESP_LOGI(TAG, "Complete frame: %.*s", frame_len, frame.data);
                            // 发送到队列（非阻塞）
                            if (xQueueSend(lora_frame_queue, &frame, 0) != pdPASS)
                            {
                                ESP_LOGE(TAG, "Send frame to queue failed");
                            }
                        }
                        // 重置帧长度，准备接收下一帧
                        frame_len = 0;
                    }
                }
                ESP_LOGI(TAG, "[DATA]: %s", uart_buffer);
                memset(uart_buffer, 0, sizeof(uart_buffer));
                break;

            // 事件类型：硬件FIFO溢出
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // 清空串口输入缓冲区，防止数据堆积
                uart_flush_input(USER_UART_NUM);
                // 重置事件队列，清除队列中未处理的事件
                xQueueReset(uart_queue);
                break;

            // 事件类型：串口环形缓冲区满
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // 提示：如果频繁出现缓冲区满，应该考虑增大缓冲区大小
                // 此处直接清空输入缓冲区，以继续接收新数据
                uart_flush_input(USER_UART_NUM);
                xQueueReset(uart_queue);
                break;

            // 事件类型：串口RX中断（接收中断）
            case UART_BREAK:
                static uint8_t count = 0;
                if (count++ % 10 == 0)
                    ESP_LOGI(TAG, "uart rx break");
                break;

            // 事件类型：串口奇偶校验错误
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;

            // 事件类型：串口帧错误
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;

            // 其他未处理的事件类型
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    // 任务退出时删除自身（此处循环不会退出，该代码不会执行）
    vTaskDelete(NULL);
}
static void linear_buf_add(linear_buf_t *b, float v)
{
    b->buf[b->idx++] = v;
    if (b->idx >= PREDICT_WIN_SIZE)
    {
        b->idx = 0;
    }
    if (b->count < PREDICT_WIN_SIZE)
    {
        b->count++;
    }
}
// 控制逻辑函数
static bool linear_predict_ex(linear_buf_t *b, predict_result_t *out)
{
    if (b->count < 4)
        return false;

    float sum_x = 0, sum_y = 0;
    float sum_xy = 0, sum_x2 = 0;

    for (int i = 0; i < b->count; i++)
    {
        float x = i;
        float y = b->buf[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    float n = b->count;
    float k = (n * sum_xy - sum_x * sum_y) /
              (n * sum_x2 - sum_x * sum_x);
    float b0 = (sum_y - k * sum_x) / n;

    float future_x = (float)PREDICT_TIME_S / SAMPLE_INTERVAL_S;

    out->predict = k * future_x + b0;
    out->slope = k;

    return true;
}
static float calc_thi(float temp, float humi)
{
    return temp + 0.36f * humi;
}

// 采集数据任务
void data_task(void *pvParameters)
{
    uint16_t soil, light;
    int temp_data = 100, humi_data = 100;
    char sendBuf[128];
    thVal.tempTh = read_Data_Threshold(NODE_TEMP_KEY);
    if (thVal.tempTh == 0)
    {
        thVal.tempTh = 320;
        write_Data_Threshold(NODE_TEMP_KEY, thVal.tempTh);
    }
    thVal.humiTh = read_Data_Threshold(NODE_HUMI_KEY);
    if (thVal.humiTh == 0)
    {
        thVal.humiTh = 50;
        write_Data_Threshold(NODE_HUMI_KEY, thVal.humiTh);
    }
    thVal.soilTh = read_Data_Threshold(NODE_SOIL_KEY);
    if (thVal.soilTh == 0)
    {
        thVal.soilTh = 350;
        write_Data_Threshold(NODE_SOIL_KEY, thVal.soilTh);
    }
    static linear_buf_t temp_buf = {0};
    static linear_buf_t soil_buf = {0};
    static linear_buf_t thi_buf = {0};
    // ESP_LOGI(TAG, "mac:%s", (char *)macId);
    // ESP_LOG_BUFFER_HEX(TAG, macId, 6);

    float thi = 0;
    float temp;
    float humi;
    float temp_pred, soil_pred;
    predict_result_t thi_pred;
    while (1)
    {
        light = bh1750_read_data() / 1.2;
        soil = adc_continuous_read_data();
        DHT11_StartGet(&temp_data, &humi_data);
        temp = temp_data / 10.0;
        humi = humi_data / 1.0;
        // 打印
        // printf("temp:%f humi:%f soil:%d light:%d\n", temp, humi, soil, light);
        // 拼 CSV 字符串
        if (node_id != 0)
        {
            snprintf(sendBuf, sizeof(sendBuf),
                     "{\"id\":%d,\"temp\":%.2f,\"humi\":%.2f,\"light\":%d,\"soil\":%d,\"fan\":%d,\"pump\":%d,\"LED\":%d}",
                     node_id,
                     temp,
                     humi,
                     light,
                     soil,
                     fpStatus.fan_status,
                     fpStatus.pump_status,
                     fpStatus.led_status);
            loraDataSendServer(sendBuf); // 发送数据
        }
        linear_buf_add(&temp_buf, temp);
        linear_buf_add(&soil_buf, soil / 1.0);
        thi = calc_thi(temp, humi);
        linear_buf_add(&thi_buf, thi);
        if (linear_predict_ex(&thi_buf, &thi_pred))
        {
            if (thi_pred.slope > THI_SLOPE_MIN && thi_pred.predict > crop->thi_high)
            {
                xEventGroupSetBits(env_event_group, EVT_ENV_WILL_BAD);
                ESP_LOGI(TAG, "will bad, slope:%f predict:%f", thi_pred.slope, thi_pred.predict);
            }
            else
            {
                xEventGroupClearBits(env_event_group, EVT_ENV_WILL_BAD);
            }
            /* 温度预测 */
            if (linear_predict_ex(&temp_buf, &temp_pred))
            {
                ESP_LOGI(TAG, "Temp predict = %.1f", temp_pred);

                if (temp_pred > thVal.tempTh / 10.0)
                {
                    xEventGroupSetBits(env_event_group, EVT_TEMP_WILL_HIGH);
                }
                else
                {
                    xEventGroupClearBits(env_event_group, EVT_TEMP_WILL_HIGH);
                }
            }

            /* 土壤预测 */
            if (linear_predict_ex(&soil_buf, &soil_pred))
            {
                ESP_LOGI(TAG, "Soil predict = %.1f", soil_pred);

                if (soil_pred < thVal.soilTh / 10.0)
                {
                    xEventGroupSetBits(env_event_group, EVT_SOIL_WILL_DRY);
                }
                else
                {
                    xEventGroupClearBits(env_event_group, EVT_SOIL_WILL_DRY);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        // ESP_LOGI(TAG, "send data:%s", sendBuf);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// 控制逻辑函数
void control_task(void *pvParameters)
{
    while (1)
    {
        system_state_t state = STATE_IDLE;
        EventBits_t bits;

        while (1)
        {
            bits = xEventGroupWaitBits(
                env_event_group,
                EVT_TEMP_HIGH | EVT_TEMP_NORMAL |
                    EVT_SOIL_DRY | EVT_SOIL_OK,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

            switch (state)
            {

            case STATE_IDLE:
                if (bits & EVT_TEMP_WILL_HIGH)
                {
                    fan_on_low(); // 低速预通风
                    state = STATE_PREDICT_COOLING;
                }
                else if (bits & EVT_SOIL_WILL_DRY)
                {
                    pump_on_pulse(); // 脉冲灌溉
                    state = STATE_PREDICT_WATERING;
                }
                break;
            case STATE_PREDICT_COOLING:
                if (bits & EVT_TEMP_HIGH)
                {
                    fan_on_high();
                    state = STATE_COOLING;
                }
                else if (bits & EVT_TEMP_NORMAL)
                {
                    fan_off();
                    state = STATE_IDLE;
                }
                break;
            case STATE_COOLING:
                if (bits & EVT_TEMP_NORMAL)
                {
                    fan_off();
                    state = STATE_IDLE;
                }
                break;
            case STATE_PREDICT_WATERING:
                if (bits & EVT_SOIL_DRY)
                {
                    pump_on();
                    state = STATE_WATERING;
                }
                else if (bits & EVT_SOIL_OK)
                {
                    pump_off();
                    state = STATE_IDLE;
                }
                break;
            case STATE_WATERING:
                if (bits & EVT_SOIL_OK)
                {
                    pump_off();
                    state = STATE_IDLE;
                }
                break;

            default:
                state = STATE_IDLE;
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

/*************************Node code end************************************** */

void init_hwrdware(void)
{
#if (!SERVER)
    bh1750_init(); // 初始化传感器
    DHT11_Init();

    adc_ts_init();
    gpio_init(); // 初始化GPIO
#else            // 初始化MQTT
    mqtt_init();
#endif
    lora_init();
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS出现错误，执行擦除
        ESP_ERROR_CHECK(nvs_flash_erase());
        // 重新尝试初始化
        ESP_ERROR_CHECK(nvs_flash_init());
    }
}

// 创建任务函数
void task_main(void)
{
    init_hwrdware();     // 初始化硬件
    nvsReadNodeIdInit(); // 读取节点ID
    FanPQueue = xQueueCreate(10, sizeof(nodeFP_t));
    if (FanPQueue == NULL)
    {
        ESP_LOGI("main", "create queue failed");
    }
    BaseType_t ret = xTaskCreatePinnedToCore(lora_task, "lora_task",
                                             4096, NULL, 10, NULL, 1);
    if (ret != pdPASS)
    {
        ESP_LOGI("lora", "create task failed");
    }
    lora_frame_queue = xQueueCreate(5, sizeof(lora_frame_t)); // 创建lora帧队列
    if (lora_frame_queue == NULL)
    {
        ESP_LOGI("main", "create queue failed");
    }
#if !SERVER
    esp_read_mac(macId, ESP_MAC_WIFI_STA); // 获取MAC地址
    snprintf(nodeIdName, 9, "node_%u", node_id);
    ret = xTaskCreatePinnedToCore(
        data_task, "data_task",
        8192, NULL, 10, NULL, 1);
    if (ret != pdPASS)
    {
        ESP_LOGI("data_task", "create task failed");
    }
    env_event_group = xEventGroupCreate();
    if (env_event_group == NULL)
    {
        ESP_LOGI("main", "create event group failed");
    }
    ret = xTaskCreate(control_task,
                      "control_task",
                      4096,
                      NULL,
                      6,
                      NULL);
    if (ret != pdPASS)
    {
        ESP_LOGI("control_task", "create task failed");
    }
    ret = xTaskCreate(loraDataClient, "loraDataClient",
                      8192, NULL, 9, NULL);
#else // 创建mqtt接收数据任务
    gateway_event_group = xEventGroupCreate();
    if (gateway_event_group == NULL)
    {
        ESP_LOGE(TAG, "创建事件组失败（内存不足）！");
        return;
    }
    ESP_LOGI(TAG, "事件组创建成功");
    ret = xTaskCreatePinnedToCore(
        mqttSendDataLoraTask, "mqttSendDataLoraTask",
        4096, NULL, 10, NULL, 1);
    if (ret != pdPASS)
    {
        ESP_LOGI("mqttSendDataLoraTask", "create task failed");
    }
    ret = xTaskCreate(add_node_task,
                      "add_node_task",
                      2048,
                      NULL,
                      5,
                      &add_node_task_handle);
    if (ret != pdPASS)
    {
        ESP_LOGI("add_node_task", "create task failed");
    }
    if (nodeNum != 0)
    {
        vTaskSuspend(add_node_task_handle);
    }
    ret = xTaskCreate(sendNodeIDTask,
                      "sendNodeIDTask",
                      2048,
                      (void *)gateway_event_group,
                      5,
                      NULL);
    if (ret != pdPASS)
    {
        ESP_LOGI("sendNodeIDTask", "create task failed");
    }
    // ret = xTaskCreate(ack_timeout_task,
    //                   "ack_timeout_task",
    //                   4096,
    //                   NULL,
    //                   5,
    //                   NULL);
    // if (ret != pdPASS)
    // {
    //     ESP_LOGI("ack_timeout_task", "create task failed");
    // }
    ret = xTaskCreate(ServerDataClient, "ServerDataClient",
                      8192, NULL, 9, NULL);
#endif
    vTaskDelete(NULL); // 删除任务自身
}

void app_main(void)
{
    task_main();
    // bh1750_main();
    // dht11_main();
    // ts_main();
}
