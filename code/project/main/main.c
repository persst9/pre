#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "driver/gpio.h" 
#include <esp_log.h>
#include <freertos/queue.h>
#include "driver/uart.h"            // ESP32的UART驱动头文件，包含串口相关函数和宏

#include "bh1750.h"
#include "dht11.h"
#include "mqtt.h"
#include "lora.h"
#include "ware.h"
#include "TS.h"
#define LORA_NUM 3

static const char* TAG = "main";
static QueueHandle_t uart_queue; //队列句柄
static uint8_t uart_buffer[1024]; // 定义UART接收缓冲区

node_data_t node_data[LORA_NUM]; // 节点数据
float temp,light;                // 温湿度
int humi,ts_humi;               // 温湿度
uint8_t node_id;                // 节点ID
void bh1750_task(void *pvParameters)
{
    
    while(1)
    {
        light = bh1750_read_data() /1.2;      // 读取光照强度
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void dht11_task(void *pvParameters)
{
    
    int temp_data, humi_data;
    while(1)
    {   
        DHT11_StartGet(&temp_data, &humi_data);
        temp = temp_data / 10.0;
        humi = humi_data;
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void ts_task(void *pvParameters)
{
    
    while(1)
    {
        ts_humi = adc_continuous_read_data();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void lora_task(void *pvParameters)
{
   uart_event_t event;             // 定义串口事件结构体，用于存储接收到的事件        // 存储缓冲区数据大小（此处未实际使用）
    // 无限循环，持续监听串口事件
    while (1)
    {
        // 从串口事件队列中接收事件，portMAX_DELAY表示永久阻塞等待，直到有事件到来
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            // 打印事件所属的串口编号，方便调试
            ESP_LOGI(TAG, "uart[%d] event:", USER_UART_NUM);
            
            // 根据事件类型进行分支处理
            switch (event.type) {
            // 事件类型：接收到数据（最核心的事件）
            case UART_DATA:
                // 打印接收到的数据长度
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                // 从串口读取指定长度的数据到缓冲区，portMAX_DELAY表示阻塞等待直到读取完成
                uart_read_bytes(USER_UART_NUM, uart_buffer, event.size, portMAX_DELAY);
                ESP_LOGI(TAG, "[DATA EVT]:");
                // 将接收到的数据回显（原样发送出去），实现串口回显功能
                uart_write_bytes(USER_UART_NUM, uart_buffer, event.size);
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

//控制逻辑函数
void control_task(void *pvParameters)
{
    
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void init_hwrdware(void)
{
    bh1750_init(); // 初始化传感器
    DHT11_Init();
    lora_init(&uart_queue);
    adc_ts_init();
    gpio_init(); // 初始化GPIO
}

//创建任务函数
void task_main(void)
{
    init_hwrdware(); // 初始化硬件
    TaskHandle_t bh1750_handle = NULL;
    BaseType_t ret = xTaskCreate(bh1750_task, "bh1750_task", 
                                2048, NULL, 10, &bh1750_handle);
    if(ret != pdPASS) {
        ESP_LOGI("bh1750", "create task failed");
    }

    TaskHandle_t dht11_handle = NULL;
    ret = xTaskCreate(dht11_task, "dht11_task",
                      2048, NULL, 10, &dht11_handle);
    if(ret != pdPASS) {
        ESP_LOGI("dht11", "create task failed");
    }
    TaskHandle_t ts_handle = NULL;
    ret = xTaskCreate(ts_task, "ts_task",
                      2048, NULL, 10, &ts_handle);
    if(ret != pdPASS) {
        ESP_LOGI("ts", "create task failed");
    }

    ret = xTaskCreatePinnedToCore(lora_task, "lora_task",
                      2048, NULL, 10, NULL, 1);
    if(ret != pdPASS) {
        ESP_LOGI("lora", "create task failed");
    }
    vTaskDelete(NULL); // 删除任务自身
}


void app_main(void)
{
    //task_main();
    mqtt_task();
}

