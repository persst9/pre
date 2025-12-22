#ifndef __WARE_H__
#define __WARE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_ON  gpio_set_level(LED_PIN,1)
#define LED_OFF gpio_set_level(LED_PIN,0)

#define FAN_ON  gpio_set_level(FAN_PIN,1)
#define FAN_OFF gpio_set_level(FAN_PIN,0)

#define MOTOR_ON  gpio_set_level(MOTOR_PIN,1)
#define MOTOR_OFF gpio_set_level(MOTOR_PIN,0)

void gpio_init(void);

#endif
