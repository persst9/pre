#ifndef __LORA_H__
#define __LORA_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define     USER_UART_NUM   UART_NUM_1
#define     USER_UART_TX_PIN  (GPIO_NUM_18)
#define     USER_UART_RX_PIN  (GPIO_NUM_17)

void lora_init(void);

void lora_send(uint8_t *data, uint16_t len);
void lora_receive(uint8_t *data, uint16_t len);

extern QueueHandle_t uart_queue;

#endif
