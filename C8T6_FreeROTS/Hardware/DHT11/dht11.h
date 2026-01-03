#ifndef __DHT11_H
#define __DHT11_H 

#include "stm32f10x.h"                  // Device header
/******************************************************************************************/
/* DHT11 引脚 定义 */
#define DHT11_DQ_GPIO_PORT                  GPIOA
#define DHT11_DQ_GPIO_PIN                   GPIO_Pin_1
#define DHT11_DQ_GPIO_CLK_ENABLE()          do{ RCC->APB2ENR |= 1 << 2; }while(0)           /* PA口时钟使能 */

/******************************************************************************************/
/* IO操作函数 */
#define DHT11_DQ_OUT(x)         GPIO_WriteBit(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN, (BitAction)(x))  /* 数据端口输出 */
#define DHT11_DQ_IN             GPIO_ReadInputDataBit(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN)     /* 数据端口输入 */

/******************************************************************************************/
/* 函数声明 */
uint8_t dht11_init(void);                               /* 初始化DHT11 */
uint8_t dht11_check(void);                              /* 检测是否存在DHT11 */
uint8_t dht11_read_data(uint8_t *temp,uint8_t *humi);   /* 读取温湿度 */

#endif