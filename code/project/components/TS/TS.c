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
#define EXAMPLE_READ_LEN  8
#define EMA_ALPHA   0.05f
// ========================== 全局变量（改为 static + 对外接口） ==========================
static const char *TAG = "TS APP";
static QueueHandle_t adc_queue = NULL;
static adc_continuous_handle_t s_adc_handle = NULL; // ADC 句柄
//GPIO5
static volatile uint16_t adc_value = 0;
static volatile uint16_t adc_filtered = 0;
static float ema_value = 0.0f;

// ========================== 中断回调函数（核心逻辑不变，仅修改作用域） ==========================
bool  adc_conv_done_cb(adc_continuous_handle_t handle,
                      const adc_continuous_evt_data_t *edata,
                      void *user_data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (int i = 0; i < edata->size; i += 2) {
        uint16_t raw = ((edata->conv_frame_buffer[i + 1] & 0x0F) << 8)
                        | edata->conv_frame_buffer[i];
        if(adc_queue)
        xQueueSendFromISR(adc_queue, &raw, &xHigherPriorityTaskWoken);
    }

    return xHigherPriorityTaskWoken == pdTRUE;
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
        .sample_freq_hz = 1 * 1000,
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

static void adc_process_task(void *arg)
{
    uint16_t raw;

    while (1) {
        if (xQueueReceive(adc_queue, &raw, portMAX_DELAY)) {

            if (ema_value == 0.0f) {
                ema_value = raw;   // 首次初始化
            } else {
                ema_value += EMA_ALPHA * ((float)raw - ema_value);
            }
            adc_filtered = (uint16_t)ema_value;
        }
    }
}

// ========================== 对外接口（供主文件调用） ==========================
void adc_ts_init(void)
{
    adc_queue = xQueueCreate(32, sizeof(uint16_t));
    if (adc_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create adc queue");
    }
    xTaskCreate(adc_process_task, "adc_process_task", 2048, NULL, 5, NULL);
    continuous_adc_init();
}



uint16_t adc_continuous_read_data(void)
{
    return adc_filtered;;
}
float convert_to(uint16_t input_value)
{
    // 1. 边界值校验（防止输入超出合理范围）
    if (input_value < 0)
    {
        // printf("错误：输入值超出范围（0~%d）\n", PEAK_VALUE);
        return -1.0f; // 返回异常值
    }

    // 2. 核心转换计算（反向线性映射）
    float percentage = 100.0f * (1.0f - (float)input_value / 1995);

    // 3. 精度处理（保留2位小数，可选）
    percentage = (float)((int)(percentage * 100 + 0.5)) / 100;
    if(input_value > 1995) {
        percentage = 0.0f;
    }
    return percentage;
}
void ts_main(void)
{
    adc_ts_init();
    float percentage = 0.0f;
    uint16_t ts_data = 0;
    while (1)
    {
        ts_data = adc_continuous_read_data();
        //延时
       percentage = convert_to(ts_data);
        ESP_LOGI(TAG, "TS percentage: %f", percentage);
        ESP_LOGI(TAG, "TS value: %u", ts_data);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}