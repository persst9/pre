#ifndef _DHT11_H_
#define _DHT11_H_
#include <stdint.h>

/** DHT11初始化
 * @param dht11_pin GPIO引脚
 * @return 无
*/
void DHT11_Init(void);

/** 获取DHT11数据
 * @param temp_x10 温度值X10
 * @param humidity 湿度值
 * @return 无
*/
int DHT11_StartGet(int *temp_x10, int *humidity);
void dht11_main(void);
#endif
