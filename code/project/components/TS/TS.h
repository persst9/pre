#ifndef __TS_H__
#define __TS_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_continuous.h"
// ========================== 回调函数声明（关键：对外暴露回调函数类型） ==========================
// 声明回调函数，供驱动注册（必须与实现一致，保留 IRAM_ATTR）
bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);

// ========================== 对外接口声明 ==========================
/**
 * @brief 读取ADC采样数据（主任务循环调用）
 */
void adc_continuous_read_data(void);
extern uint8_t adc_data; // 采样数据
#endif
