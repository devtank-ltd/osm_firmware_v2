#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Library includes. */
#include <libopencm3/stm32/rcc.h>

#define configUSE_PREEMPTION        1
#define configUSE_IDLE_HOOK     0
#define configUSE_TICK_HOOK     0
#define configCPU_CLOCK_HZ      ( ( unsigned long ) rcc_ahb_frequency )
#define configSYSTICK_CLOCK_HZ      ( ( portTickType ) 1000 )
#define configTICK_RATE_HZ      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES        ( 5 )
#define configMINIMAL_STACK_SIZE    ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE       ( ( size_t ) ( 32 * 1024 ) )
#define configMAX_TASK_NAME_LEN     ( 16 )
#define configUSE_TRACE_FACILITY    0
#define configUSE_16_BIT_TICKS      0
#define configIDLE_SHOULD_YIELD     1
#define configUSE_MUTEXES       1

#define configCHECK_FOR_STACK_OVERFLOW  1

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES       0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet    0
#define INCLUDE_uxTaskPriorityGet   0
#define INCLUDE_vTaskDelete     0
#define INCLUDE_vTaskCleanUpResources   0
#define INCLUDE_vTaskSuspend        0
#define INCLUDE_vTaskDelayUntil     0
#define INCLUDE_vTaskDelay      1

/*

CM4 has 8 bits for priority, lowest number is highest priority

configKERNEL_INTERRUPT_PRIORITY should be set to the lowest priority.
configMAX_SYSCALL_INTERRUPT_PRIORITY sets the highest interrupt priority
  from which interrupt safe FreeRTOS API functions can be called.

 Highest :
           configMAX_SYSCALL_INTERRUPT_PRIORITY

 Lowest :  configKERNEL_INTERRUPT_PRIORITY

*/
#define configKERNEL_INTERRUPT_PRIORITY         254
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191

#define UART1_PRIORITY configMAX_SYSCALL_INTERRUPT_PRIORITY-1
#define UART2_PRIORITY configMAX_SYSCALL_INTERRUPT_PRIORITY-2
#define UART3_PRIORITY configMAX_SYSCALL_INTERRUPT_PRIORITY-3
#define UART4_PRIORITY configMAX_SYSCALL_INTERRUPT_PRIORITY-4
#define USB_PRIORITY   configMAX_SYSCALL_INTERRUPT_PRIORITY-5


#define vPortSVCHandler     sv_call_handler
#define xPortPendSVHandler  pend_sv_handler
#define xPortSysTickHandler sys_tick_handler

#endif /* FREERTOS_CONFIG_H */
