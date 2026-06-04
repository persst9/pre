#ifndef __WARE_H__
#define __WARE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define FAN_PIN   GPIO_NUM_21
#define LED_PIN   GPIO_NUM_42
#define MOTOR_PIN GPIO_NUM_39
#define LED_ON  gpio_set_level(LED_PIN,1)
#define LED_OFF gpio_set_level(LED_PIN,0)

#define FAN_ON  gpio_set_level(FAN_PIN,1)
#define FAN_OFF gpio_set_level(FAN_PIN,0)

#define PUMP_ON  gpio_set_level(MOTOR_PIN,1)
#define PUMP_OFF gpio_set_level(MOTOR_PIN,0)
void gpio_init(void);
void gpio_main_test(void);
#endif
