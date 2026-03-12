#include "lora.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "mqtt.h"
#define receive_buffer_size 1024
#define send_buffer_size 1024
#define baud_rate_num 9600


uint8_t send_head = 0x55;
uint8_t send_tail = 0xAA;

static const char *TAG = "lora"; // 定义日志标签
QueueHandle_t uart_queue = NULL; // 队列句柄
/**
 * @brief  初始化UART1串口通信配置
 * @note   配置参数：9600波特率、8位数据位、1位停止位、无校验、无硬件流控
 * @param  无
 * @retval 无
 */
void lora_init(void)
{
    // 定义UART配置结构体并初始化参数
    uart_config_t   uart_structure = {
        .baud_rate = baud_rate_num,               // 串口波特率：9600
        .data_bits = UART_DATA_8_BITS,    // 数据位：8位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 硬件流控：禁用（无RTS/CTS）
        .parity = UART_PARITY_DISABLE,    // 校验位：禁用（无奇偶校验）
        .rx_flow_ctrl_thresh = 122,       // 接收流控阈值：122（禁用流控时此参数无实际作用）
        .source_clk = UART_SCLK_APB,      // 串口时钟源：APB时钟
        .stop_bits = UART_STOP_BITS_1,    // 停止位：1位
    };

    // 配置UART1的参数（将上述结构体配置应用到UART_NUM_1）
    uart_param_config(USER_UART_NUM, &uart_structure);

    /**
     * 配置UART1的引脚映射
     * 参数说明：
     * - UART_NUM_1：目标串口编号
     * - GPIO_NUM_17：TX发送引脚
     * - GPIO_NUM_18：RX接收引脚
     * - -1：RTS引脚（禁用硬件流控，设为-1）
     * - -1：CTS引脚（禁用硬件流控，设为-1）
     */
    uart_set_pin(USER_UART_NUM, USER_UART_TX_PIN, USER_UART_RX_PIN, -1, -1);
    //gpio_set_pull_mode(USER_UART_RX_PIN, GPIO_PULLUP_ONLY);

    /**
     * 安装UART驱动并分配缓冲区
     * 参数说明：
     * - UART_NUM_1：目标串口编号
     * - 1024：接收缓冲区大小（字节）
     * - 1024：发送缓冲区大小（字节）
     * - 20：UART事件队列大小（用于接收中断事件）
     * - NULL：事件队列句柄（不使用则设为NULL）
     * - 0：驱动安装标志（无特殊标志）
     */
    uart_driver_install(USER_UART_NUM, receive_buffer_size , send_buffer_size , 
                        20, &uart_queue, 0);
    uint8_t AT[10] = "+++";
    lora_send(AT, 10); // 发送AT指令
    lora_receive(AT, 10); // 接收AT指令的响应
    if(AT[0] == 'O' && AT[1] == 'K')
    {
        AT[0] = 'A';
        AT[1] = 'T';
        AT[2] = '&';
        AT[3] = 'F';
        AT[4] = '/r';
        AT[5] = '/n';
        lora_send(AT, 10); // 发送AT指令
    }
}

void lora_send(uint8_t *data, uint16_t len)
{
    uart_write_bytes(USER_UART_NUM, (char *)data, len); // 发送数据
}

void lora_receive(uint8_t *data, uint16_t len)
{
    uart_read_bytes(USER_UART_NUM, data, len, 100); // 接收数据
}
