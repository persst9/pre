#ifndef __BH1750_H_
#define __BH1750_H_

#include <stdint.h>

#define PowerDown               0x00
#define PowerOn                 0x01
#define Reset                   0x07
#define HResolutionMode         0x10
#define HResolutionMode2        0x11
#define LResolutionMode         0x13
#define OneTimeHResolutionMode  0x20
#define OneTimeHResolutionMode2 0x21
#define OneTimeLResolutionMode  0x23

void bh1750_send_cmd(uint8_t cmd_data);
uint16_t bh1750_read_data(void);
void bh1750_init(void);
void bh1750_main(void);
#endif