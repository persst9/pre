#include "bh1750.h"
#include "FreeRTOS.h"
#include "task.h"
#include "iic.h"
#define bh1750_addr 0x23
#define bh1750_write_addr 0x46
#define bh1750_read_addr  0x47

static float light = 0; 

void bh1750_send_cmd(uint8_t cmd_data)
{
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, bh1750_write_addr, true);
    // i2c_master_write_byte(cmd, cmd_data, true);
    // i2c_master_stop(cmd);
    // i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000);
    // i2c_cmd_link_delete(cmd);

    MyI2C_Start();						//I2C起始信号
    MyI2C_SendByte(bh1750_addr     );  //发送设备地址+写命令
    MyI2C_ReceiveAck();					//接收应答信号
    MyI2C_SendByte(bh1750_write_addr);  //发送设备地址+写命令
    MyI2C_ReceiveAck();					//接收应答信号
    MyI2C_SendByte(cmd_data);			//发送数据
    MyI2C_ReceiveAck();					//接收应答信号
    MyI2C_Stop();						//I2C停止信号
}

uint16_t bh1750_read_data(void)
{
    uint8_t light_high = 0, light_low = 0;
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, bh1750_read_addr, true);
    // i2c_master_read_byte(cmd, &light_high, I2C_MASTER_ACK);
    // i2c_master_read_byte(cmd, &light_low, I2C_MASTER_ACK);
    // i2c_master_stop(cmd);
    // i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000);
    // i2c_cmd_link_delete(cmd);
    // return light_high<<8|light_low;
    MyI2C_Start();						//I2C起始信号
    MyI2C_SendByte(bh1750_addr);  //发送设备地址+写命令
    MyI2C_ReceiveAck();					//接收应答信号
    MyI2C_SendByte(bh1750_read_addr);  //发送设备地址+写命令
    MyI2C_ReceiveAck();					//接收应答信号
    MyI2C_Stop();						//I2C停止信号
    MyI2C_Start();						//I2C起始信号
    MyI2C_SendByte(bh1750_addr|0x01);  //发送设备地址+读命令
    MyI2C_ReceiveAck();					//接收应答信号
    light_high = MyI2C_ReceiveByte();	//读取数据
    light_low = MyI2C_ReceiveByte();	//读取数据
    MyI2C_SendAck(1);					//发送应答，给从机非应答，终止从机的数据输出
	MyI2C_Stop();						//I2C终止信号
    return light_high<<8|light_low;
}

void bh1750_init(void)
{
    MyI2C_Init();
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