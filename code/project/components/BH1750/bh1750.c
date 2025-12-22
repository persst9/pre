#include "bh1750.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "iic.h"
#include <esp_log.h>
#define bh1750_write_addr 0x46
#define bh1750_read_addr  0x47

static float light = 0 ; 

void bh1750_send_cmd(uint8_t cmd_data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, bh1750_write_addr, true);
    i2c_master_write_byte(cmd, cmd_data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000);
    i2c_cmd_link_delete(cmd);
}

uint16_t bh1750_read_data(void)
{
    uint8_t light_high = 0, light_low = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, bh1750_read_addr, true);
    i2c_master_read_byte(cmd, &light_high, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &light_low, I2C_MASTER_ACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    return light_high<<8|light_low;
}

void bh1750_init(void)
{
    iic_init();
    bh1750_send_cmd(PowerOn);
    bh1750_send_cmd(HResolutionMode);
    vTaskDelay(200);
}


void bh1750_main(void)
{
    bh1750_init();
    
    while (1)
    {
        light = bh1750_read_data() / 1.2;
        ESP_LOGI("light", "light: %f", light);
        vTaskDelay(500);
    }
}