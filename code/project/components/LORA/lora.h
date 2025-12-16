#ifndef __LORA_H__
#define __LORA_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void lora_init(void);

void lora_send(uint8_t *data, uint16_t len);
void lora_receive(uint8_t *data, uint16_t len);


#endif
