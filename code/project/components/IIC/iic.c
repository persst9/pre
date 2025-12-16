#include "iic.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#define SCL_PIN GPIO_NUM_6
#define SDA_PIN GPIO_NUM_7

void iic_init(void)
{
    i2c_config_t i2c_structure = {
        .clk_flags = 0,
        .master.clk_speed = 100000,
        .mode = I2C_MODE_MASTER,
        .scl_io_num = SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .sda_io_num = SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
    };
    i2c_param_config(I2C_NUM_0, &i2c_structure);

    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}