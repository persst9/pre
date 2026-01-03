/*
 * FreeRTOS V202212.01
 * 版权所有 (C) 2020 Amazon.com, Inc. 或其附属机构。保留所有权利。
 *
 * 允许任何人免费获取本软件及其相关文档文件（“软件”）的副本，
 * 并可以无限制地处理软件，包括但不限于使用、复制、修改、合并、发布、分发、 sublicense和/或销售软件副本，
 * 并允许向其提供软件的人员这样做，但需遵守以下条件：
 *
 * 上述版权声明和本许可声明应包含在软件的所有副本或实质性部分中。
 *
 * 软件按“原样”提供，不提供任何形式的保证，无论是明示的还是暗示的，
 * 包括但不限于适销性、特定用途适用性和非侵权性的保证。
 * 在任何情况下，作者或版权所有者均不对任何索赔、损害或其他责任负责，
 * 无论是因合同、侵权或其他原因引起的，还是与软件或其使用或其他交易有关的。
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*---------------------------------------------
 * 应用程序特定配置
 *
 * 这些定义需根据具体硬件和应用需求调整
 * 相关参数说明见 FreeRTOS 官网 API 文档的“配置”部分
 * 参考：http://www.freertos.org/a00110.html
----------------------------------------------*/

// 启用抢占式调度（1=启用，0=禁用）
#define configUSE_PREEMPTION        1


/***************************************************************
 * FreeRTOS 钩子函数配置（Hook Functions）
 **************************************************************/

/*---------------------------------------------------------------------
 * 空闲钩子函数（Idle Hook）
 * 功能：在 CPU 进入空闲任务时执行自定义操作（如低功耗模式）
 * 配置：
 * - 1：启用空闲钩子，需实现 void vApplicationIdleHook(void)
 * - 0：禁用（默认）
 * 说明：空闲任务是 RTOS 必须创建的最低优先级任务，用于处理 CPU 无其他任务时的空闲状态
-----------------------------------------------------------------------*/
#define configUSE_IDLE_HOOK         0  // 开发阶段建议设为 0，调试完成后可启用以实现低功耗

/*---------------------------------------------------------------------
 * 时钟节拍钩子函数（Tick Hook）
 * 功能：在系统时钟节拍中断处理后执行自定义操作
 * 配置：
 * - 1：启用时钟节拍钩子，需实现 void vApplicationTickHook(void)
 * - 0：禁用（默认）
 * 说明：钩子函数在 xPortSysTickHandler 中断处理末尾调用，属于中断上下文
-----------------------------------------------------------------------*/
#define configUSE_TICK_HOOK         0

// 启用内存申请失败钩子（1=启用，需实现 vApplicationMallocFailedHook()）
#define configUSE_MALLOC_FAILED_HOOK        0 


/***************************************************************
 * 系统时钟与任务调度配置
 **************************************************************/

// CPU 主频（单位：Hz），此处配置为 72MHz
#define configCPU_CLOCK_HZ          ((unsigned long)72000000)
// 系统时钟节拍频率（单位：Hz），1000Hz 表示每秒产生 1000 个时钟节拍
#define configTICK_RATE_HZ          ((TickType_t)1000)
// 最大任务优先级数量（0 为最低优先级），支持 0~31 共 32 个优先级
#define configMAX_PRIORITIES        32  
// 最小任务栈大小（单位：字，每个字通常为 4 字节），即 128*4=512 字节
#define configMINIMAL_STACK_SIZE    ((unsigned short)128)  
// 堆内存总大小（单位：字节），用于动态内存管理，此处配置为 17KB
#define configTOTAL_HEAP_SIZE       ((size_t)(17 * 1024))
// 任务名称最大长度（字符数），超过会被截断
#define configMAX_TASK_NAME_LEN     16
// 启用跟踪功能（用于可视化调试工具），0=禁用
#define configUSE_TRACE_FACILITY    0
// 时钟节拍计数器类型：0=32位（默认），1=16位（节省内存但范围有限）
#define configUSE_16_BIT_TICKS      0
/*---------------------------------------------------------------------
 * 空闲任务让步策略
 * 配置：
 * - 1：当空闲任务运行时，若有同优先级就绪任务则主动让出 CPU
 * - 0：空闲任务不主动让步（仅适用于非抢占模式）
-----------------------------------------------------------------------*/
#define configIDLE_SHOULD_YIELD     1 
// 启用时间片轮转调度（1=对同优先级任务启用时间片）
#define configUSE_TIME_SLICING      1 

// 启用队列集功能（1=允许组合多个队列进行原子操作）
#define configUSE_QUEUE_SETS        0 

// 启用任务通知功能（1=默认启用，任务间轻量级通信机制）
#define configUSE_TASK_NOTIFICATIONS 1 

// 启用互斥信号量（1=支持带优先级继承的互斥锁）
#define configUSE_MUTEXES           0  
// 启用递归互斥信号量（1=支持可重入的互斥锁）
#define configUSE_RECURSIVE_MUTEXES 0  
// 启用计数信号量（1=支持计数型信号量）
#define configUSE_COUNTING_SEMAPHORES 0  

// 队列注册表大小（用于调试时记录队列信息，0=禁用）
#define configQUEUE_REGISTRY_SIZE   10  
// 启用任务标签功能（1=允许为任务添加自定义标签）
#define configUSE_APPLICATION_TASK_TAG 0  


/***************************************************************
 * 内存分配策略配置
 **************************************************************/

// 支持动态内存分配（1=使用 pvPortMalloc/pvPortFree 动态创建任务/队列）
#define configSUPPORT_DYNAMIC_ALLOCATION    1    
// 支持静态内存分配（1=使用静态数组预先分配任务/队列内存）
#define configSUPPORT_STATIC_ALLOCATION     0    
/*---------------------------------------------------------------------
 * 栈溢出检测等级
 * 配置：
 * - 0：禁用检测（性能最佳）
 * - 1：简单检测（通过检查栈末尾标记字）
 * - 2：详细检测（每次任务切换时检查栈内容，开销较大）
-----------------------------------------------------------------------*/
#define configCHECK_FOR_STACK_OVERFLOW      0  


/***************************************************************
 * 运行时统计与跟踪配置
 **************************************************************/

// 生成运行时统计信息（1=启用，需配合 configUSE_TRACE_FACILITY）
#define configGENERATE_RUN_TIME_STATS      0    
// 启用跟踪功能（配合可视化工具如 FreeRTOS+Trace）
#define configUSE_TRACE_FACILITY           0    


/***************************************************************
 * 协程（Co-Routine）配置（ deprecated，建议使用任务替代）
 **************************************************************/

// 启用协程功能（0=禁用，FreeRTOS v10.0.0 后不推荐使用）
#define configUSE_CO_ROUTINES             0     
// 协程最大优先级数量（仅在启用协程时有效）
#define configMAX_CO_ROUTINE_PRIORITIES   2      


/***************************************************************
 * 软件定时器配置
 **************************************************************/

// 启用软件定时器功能（1=创建独立的定时器服务任务）
#define configUSE_TIMERS                  0       
// 定时器任务优先级（建议设为最高优先级或次高）
#define configTIMER_TASK_PRIORITY         (configMAX_PRIORITIES-1)  
// 定时器消息队列长度（用于缓存定时器事件）
#define configTIMER_QUEUE_LENGTH          10      
// 定时器任务栈大小（单位：字）
#define configTIMER_TASK_STACK_DEPTH      (configMINIMAL_STACK_SIZE*2)  


/***************************************************************
 * 硬件优化配置
 **************************************************************/

/*---------------------------------------------------------------------
 * 任务选择硬件优化
 * 配置：
 * - 1：启用硬件优化的任务选择算法（需 CPU 支持前导零计数指令 CLZ）
 * - 0：使用软件算法（兼容性更好但性能稍低）
 * 说明：STM32 等 ARM Cortex-M 系列 CPU 支持 CLZ 指令时建议设为 1
-----------------------------------------------------------------------*/
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1

/*---------------------------------------------------------------------
 * 无 tick 低功耗模式（Tickless Idle）
 * 配置：
 * - 1：启用无 tick 模式，系统空闲时自动关闭时钟节拍以降低功耗
 * - 0：禁用（默认，时钟节拍持续运行）
 * 注意：启用前需确保硬件支持低功耗唤醒机制（如 RTC 唤醒）
-----------------------------------------------------------------------*/
#define configUSE_TICKLESS_IDLE   0


/***************************************************************
 * 中断优先级配置（针对 ARM Cortex-M 架构）
 **************************************************************/

// 使用 CMSIS 定义的 NVIC 优先级分组位数（若未定义则默认 4 位）
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS        __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS        4                  
#endif
// 最低中断优先级（数值越大优先级越低，Cortex-M 最小为 0）
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15     
// 系统调用可管理的最高中断优先级（优先级低于此值的中断可被 FreeRTOS 管理）
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5      
// 内核中断优先级（用于 PendSV/SVC 等系统中断）
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))  /* 240 */
// 最大系统调用中断优先级（转换为实际寄存器值）
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

// ST 库兼容配置（匹配 configKERNEL_INTERRUPT_PRIORITY）
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY    15


/***************************************************************
 * 内核函数包含配置（按需裁剪以减小代码体积）
 **************************************************************/

// 包含获取调度器状态函数 xTaskGetSchedulerState()
#define INCLUDE_xTaskGetSchedulerState       1                       
// 包含设置任务优先级函数 vTaskPrioritySet()
#define INCLUDE_vTaskPrioritySet             1
// 包含获取任务优先级函数 uxTaskPriorityGet()
#define INCLUDE_uxTaskPriorityGet            1
// 包含删除任务函数 vTaskDelete()
#define INCLUDE_vTaskDelete                  1
// 包含任务资源清理函数 vTaskCleanUpResources()
#define INCLUDE_vTaskCleanUpResources        1
// 包含挂起任务函数 vTaskSuspend()
#define INCLUDE_vTaskSuspend                 1
// 包含阻塞直到指定时间函数 vTaskDelayUntil()
#define INCLUDE_vTaskDelayUntil              1
// 包含延迟函数 vTaskDelay()
#define INCLUDE_vTaskDelay                   1
// 包含获取任务状态函数 eTaskGetState()
#define INCLUDE_eTaskGetState                1
// 包含定时器pend函数调用 xTimerPendFunctionCall()（需启用定时器）
#define INCLUDE_xTimerPendFunctionCall       0
// 以下为可选功能（默认注释，需用时取消注释）
// #define INCLUDE_xTaskGetCurrentTaskHandle    1  // 获取当前任务句柄
// #define INCLUDE_uxTaskGetStackHighWaterMark  0  // 获取栈使用峰值
// #define INCLUDE_xTaskGetIdleTaskHandle       0  // 获取空闲任务句柄


// 硬件中断服务函数别名（适配 STM32 等 Cortex-M 平台）
#define xPortPendSVHandler              PendSV_Handler 
#define vPortSVCHandler                 SVC_Handler

#endif /* FREERTOS_CONFIG_H */
