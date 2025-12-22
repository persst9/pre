#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "driver/gpio.h" 
#include <esp_log.h>
#include <freertos/queue.h>
#include "bh1750.h"
#include "dht11.h"
#include "mqtt.h"
#include "lora.h"
#include "ware.h"
#include "TS.h"
#define LORA_NUM 3

static const char* TAG = "main";

node_data_t node_data[LORA_NUM];
float temp,light;
int humi,ts_humi;
uint8_t node_id;
void bh1750_task(void)
{
    
    while(1)
    {
        light = bh1750_read_data() /1.2;      // 读取光照强度
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void dht11_task(void)
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

void ts_task(void)
{
    
    while(1)
    {
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void lora_task(void)
{
    
    while(1)
    {
        ts_humi = adc_continuous_read_data();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}

void control_task(void)
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
    lora_init();
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
    TaskHandle_t lora_handle = NULL;
    ret = xTaskCreate(lora_task, "lora_task",
                      2048, NULL, 10, &lora_handle);
    if(ret != pdPASS) {
        ESP_LOGI("lora", "create task failed");
    }
    vTaskDelete(NULL); // 删除任务自身
}


void app_main(void)
{
    task_main();
}

