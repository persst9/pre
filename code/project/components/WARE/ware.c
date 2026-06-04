#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ware.h"
static const char* TAG = "gpio_init";

void gpio_init(void)
{
        /**
    //  * 功能说明：将指定GPIO引脚初始化为输入模式，启用内部上拉电阻，禁用下拉电阻和中断功能
    //  */
    // // 定义GPIO配置结构体变量，该结构体是ESP-IDF中标准化的GPIO配置方式
    gpio_config_t gpio_structure = {
        .intr_type = GPIO_INTR_DISABLE,  // 禁用GPIO中断功能（输入模式下无需中断时设置）
        .mode = GPIO_MODE_OUTPUT,         // 设置GPIO工作模式为输入模式（GPIO_MODE_OUTPUT为输出）
        // 1ull<<GPIO_NUM_X：将1左移对应引脚号的位数，通过|（或运算）合并多个引脚
        .pin_bit_mask = (1ull<<LED_PIN)|(1ull<<MOTOR_PIN),
        .pull_down_en = GPIO_PULLUP_ENABLE,  // 禁用GPIO内部下拉电阻
        .pull_up_en = GPIO_PULLDOWN_DISABLE,       // 启用GPIO内部上拉电阻（输入模式常用，防止电平悬空）
    };
    // 调用GPIO配置函数，将上述配置应用到硬件中
    gpio_config(&gpio_structure);
    //翻转GPIO状态
    gpio_config_t gpio_structure1 = {
        .intr_type = GPIO_INTR_DISABLE,  // 禁用GPIO中断功能（输入模式下无需中断时设置）
        .mode = GPIO_MODE_INPUT_OUTPUT,         // 设置GPIO工作模式为输入模式（GPIO_MODE_OUTPUT为输出）
        // 1ull<<GPIO_NUM_X：将1左移对应引脚号的位数，通过|（或运算）合并多个引脚
        .pin_bit_mask = 1ull<<FAN_PIN,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用GPIO内部下拉电阻
        .pull_up_en = GPIO_PULLUP_DISABLE,       // 启用GPIO内部上拉电阻（输入模式常用，防止电平悬空）
    };
    gpio_config(&gpio_structure1);
    gpio_set_level(FAN_PIN, 0);
    gpio_set_level(LED_PIN, 0);
}
void gpio_main_test(void)
{
    gpio_init();
    while(1)
    {
        gpio_set_level(LED_PIN, 1);
        ESP_LOGI(TAG, "GPIO level: %d", gpio_get_level(LED_PIN));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 0);
        ESP_LOGI(TAG, "GPIO level: %d", gpio_get_level(LED_PIN));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}