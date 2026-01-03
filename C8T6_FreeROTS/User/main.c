#include "stm32f10x.h"                  // Device header
//外设初始化
#include "LED.h"
#include "Usart.h"
//end
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>


//任务优先级
#define START_TASK_PRIO		1
//任务堆栈大小	
#define START_STK_SIZE 		128  
//任务句柄
TaskHandle_t StartTask_Handler;
//任务函数
void start_task(void *pvParameters);

//任务优先级
#define LED1_TASK_PRIO		    2
#define Serial_TASK_PRIO		2
//任务堆栈大小	
#define LED1_STK_SIZE 		50  
#define Serial_STK_SIZE 	50  

//任务句柄
TaskHandle_t LED1Task_Handler;
TaskHandle_t SerialTask_Handler;
//任务函数
void led1_task(void *pvParameters);
void Serial_task(void *pvParameters);


/*******************************************************************************
* 函 数 名         : main
* 函数功能		   : 主函数
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
int main()
{
	LED_Init();
	Serial_Init();
    BaseType_t xReturn = pdPASS;
	//创建开始任务
   xReturn = xTaskCreate((TaskFunction_t )start_task,            //任务函数
                (const char*    )"start_task",          //任务名称
                (uint16_t       )START_STK_SIZE,        //任务堆栈大小
                (void*          )NULL,                  //传递给任务函数的参数
                (UBaseType_t    )START_TASK_PRIO,       //任务优先级
                (TaskHandle_t*  )&StartTask_Handler);   //任务句柄  
    if(xReturn == pdPASS)
    vTaskStartScheduler();          //开启任务调度
}

//开始任务任务函数
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           //进入临界区
    BaseType_t xReturn = pdPASS;
    //创建LED1任务
    xReturn = xTaskCreate((TaskFunction_t )led1_task,     
                (const char*    )"led1_task",   
                (uint16_t       )LED1_STK_SIZE, 
                (void*          )NULL,
                (UBaseType_t    )LED1_TASK_PRIO,
                (TaskHandle_t*  )&LED1Task_Handler); 
	 if(xReturn == pdPASS)     printf("创建LED_Task任务成功!\r\n");
    xReturn = xTaskCreate((TaskFunction_t )Serial_task,
                            "Serial_task",
                            Serial_STK_SIZE,
                            NULL,
                            Serial_TASK_PRIO,
                            (TaskHandle_t*  )&SerialTask_Handler);  
	 if(xReturn == pdPASS)     printf("创建Serial_Task任务成功!\r\n");          
    vTaskDelete(StartTask_Handler); //删除开始任务
    taskEXIT_CRITICAL();            //退出临界区
} 

//LED1任务函数
void led1_task(void *pvParameters)
{
    while(1)
    {
        LED1_ON;
        printf("LED1_ON!\r\n");
        vTaskDelay(1000);
        LED1_OFF;
        printf("LED1_OFF!\r\n");
        vTaskDelay(1000);
    }
}

void Serial_task(void *pvParameters)
{
    while(1)
    {
        if(Serial_RxFlag == 1)
        {
            Serial_RxFlag = 0;
            printf("%s",Serial_RxPacket);
        }
    }
}
