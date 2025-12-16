#include "TS.h"
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "hal/adc_types.h"

// ========================== 配置参数（可通过头文件宏定义适配） ==========================
#define EXAMPLE_READ_LEN                8
// ========================== 全局变量（改为 static + 对外接口） ==========================
static const char *TAG = "TS";
static TaskHandle_t s_task_handle = NULL;  // 主任务句柄（跨文件访问需提供 setter）
static adc_continuous_handle_t s_adc_handle = NULL; // ADC 句柄

uint8_t *data_value;
uint8_t adc_data,adc_value;
uint16_t adc_sum = 0;
uint8_t adc_channelnum;
uint8_t adc_num = 0; // ADC 采样次数
// ========================== 中断回调函数（核心逻辑不变，仅修改作用域） ==========================
bool  adc_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    data_value = edata->conv_frame_buffer;
    if(edata->size > 0)
    {
        adc_data = ((data_value[1]&0x0f) << 8) | data_value[0];
        adc_channelnum = data_value[1] >> 5 ;
        adc_num++;
        adc_sum += adc_data;
        if(adc_num == 100)
        {
            adc_num = 0;
            adc_value = (adc_sum/100);
            adc_sum = 0;
        }
        return true;
    }
    return false;
}
// ========================== 私有函数（仍用 static） =========================
static void continuous_adc_init(void)
{
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &s_adc_handle));

    adc_digi_pattern_config_t adc_digi_pattern[1] = {
        {.atten = ADC_ATTEN_DB_11,
        .channel = ADC_CHANNEL_4,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,}
    };

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
        .adc_pattern = adc_digi_pattern,
        .pattern_num = 1,
    };
    ESP_ERROR_CHECK(adc_continuous_config(s_adc_handle, &dig_cfg));
    // 注册回调（核心：绑定改造后的回调函数）
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adc_conv_done_cb,  // 直接引用外部可见的回调函数
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(s_adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(s_adc_handle));
}

// ========================== 对外接口（供主文件调用） ==========================
void adc_continuous_read_data(void)
{
    continuous_adc_init();
    while(1)
    {
        if(adc_value != 0)
        {
            ESP_LOGI(TAG, "ADC value: %u", adc_value);
            adc_value = 0;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
