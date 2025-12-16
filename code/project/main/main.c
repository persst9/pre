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

float light = 0 ;

void app_main(void)
{
    bh1750_init();
    
    while (1)
    {
        light = bh1750_read_data() / 1.2;
        ESP_LOGI("light", "light: %f", light);
        vTaskDelay(500);
    }
}
