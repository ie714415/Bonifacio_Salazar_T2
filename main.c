#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK66F18.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */

/* TODO: insert other definitions and declarations here. */
#define RTOS_STACK_SIZE 500

#define RTOS_MAX_NUMBER_OF_TASKS 3

#define STACK_FRAME_SIZE 8

#define STACK_PC_OFFSET 2

#define STACK_PSR_OFFSET 1

#define STACK_PSR_DEFAULT 0x01000000

#define NTASK 4

static uint8_t priorArray[NTASK];

typedef enum
{
  stateReady,
  stateRunning,
  stateWaiting
} rtosTaskState_t;

typedef enum
{
  from_execution,
  from_isr
} rtosContextSwitchFrom_t;

typedef struct
{
  uint32_t *sp;
  void (*task_body)();
  uint32_t stack[RTOS_STACK_SIZE];
  uint64_t local_tick;
  rtosTaskState_t state;
  uint8_t priority;
} task_t;

struct
{
  uint8_t nTask;
  int8_t current_task;
  int8_t next_task;
  task_t tasks[RTOS_MAX_NUMBER_OF_TASKS + 1];
  uint64_t global_tick;
} task_list =
{ 0 };

void task0(void);

void task1(void);

void task2(void);

void taskIdel(void);

void sortPriorities(void);

void rtosStart(void);

void rtosReloadSysTick(void);

void rtosDelay(uint64_t ticks);

void rtosActivateWaitingTask(void);

void rtosKernel(rtosContextSwitchFrom_t from);

int main(void)
{
  gpio_pin_config_t led_config =
  { kGPIO_DigitalOutput, 1 };

  /* Init board hardware. */
  BOARD_InitBootPins();
  BOARD_InitBootClocks();
  BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
  /* Init FSL debug console. */
  BOARD_InitDebugConsole();
#endif
  CLOCK_EnableClock(kCLOCK_PortC);
  PORT_SetPinMux(BOARD_LED_RED_GPIO_PORT, BOARD_LED_RED_PIN, kPORT_MuxAsGpio);
  GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_PIN, &led_config);

  CLOCK_EnableClock(kCLOCK_PortE);
  PORT_SetPinMux(BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_PIN, kPORT_MuxAsGpio);
  GPIO_PinInit(BOARD_LED_GREEN_GPIO, BOARD_LED_GREEN_PIN, &led_config);

  CLOCK_EnableClock(kCLOCK_PortA);
  PORT_SetPinMux(BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_PIN, kPORT_MuxAsGpio);
  GPIO_PinInit(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_PIN, &led_config);

  task_list.tasks[0].task_body = task0;
  task_list.tasks[0].sp = &(task_list.tasks[0].stack[RTOS_STACK_SIZE - 1])
    - STACK_FRAME_SIZE;
  task_list.tasks[0].stack[RTOS_STACK_SIZE - STACK_PC_OFFSET] =
    (uint32_t) task0;
  task_list.tasks[0].stack[RTOS_STACK_SIZE - STACK_PSR_OFFSET] =
    (STACK_PSR_DEFAULT);
  task_list.tasks[0].state = stateReady;
  task_list.nTask++;
  task_list.tasks[0].priority = 0;

  task_list.tasks[1].task_body = task1;
  task_list.tasks[1].sp = &(task_list.tasks[1].stack[RTOS_STACK_SIZE - 1])
    - STACK_FRAME_SIZE;
  task_list.tasks[1].stack[RTOS_STACK_SIZE - STACK_PC_OFFSET] =
    (uint32_t) task1;
  task_list.tasks[1].stack[RTOS_STACK_SIZE - STACK_PSR_OFFSET] =
    (STACK_PSR_DEFAULT);
  task_list.tasks[1].state = stateReady;
  task_list.nTask++;
  task_list.tasks[1].priority = 2;

  task_list.tasks[2].task_body = task2;
  task_list.tasks[2].sp = &(task_list.tasks[2].stack[RTOS_STACK_SIZE - 1])
    - STACK_FRAME_SIZE;
  task_list.tasks[2].stack[RTOS_STACK_SIZE - STACK_PC_OFFSET] =
    (uint32_t) task2;
  task_list.tasks[2].stack[RTOS_STACK_SIZE - STACK_PSR_OFFSET] =
    (STACK_PSR_DEFAULT);
  task_list.tasks[2].state = stateReady;
  task_list.nTask++;
  task_list.tasks[2].priority = 1;

  /* idle task*/
  task_list.tasks[task_list.nTask].task_body = taskIdel;
  task_list.tasks[task_list.nTask].sp =
    &(task_list.tasks[task_list.nTask].stack[RTOS_STACK_SIZE - 1])
      - STACK_FRAME_SIZE;
  task_list.tasks[task_list.nTask].stack[RTOS_STACK_SIZE - STACK_PC_OFFSET] =
    (uint32_t) taskIdel;
  task_list.tasks[task_list.nTask].stack[RTOS_STACK_SIZE - STACK_PSR_OFFSET] =
    (STACK_PSR_DEFAULT);
  task_list.tasks[task_list.nTask].state = stateReady;
  task_list.tasks[task_list.nTask].priority = 4;

  sortPriorities();
  NVIC_SetPriority(PendSV_IRQn, 0xFF);

  PRINTF("RTOS Init\n\r");

  while (1)
  {
    rtosStart();

    __asm volatile ("nop");
  }

  return 0;
}

void task0(void) // LED Red
{
  while (1)
  {
    GPIO_PortClear(BOARD_LED_RED_GPIO, 1u << BOARD_LED_RED_GPIO_PIN);
    GPIO_PortSet(BOARD_LED_GREEN_GPIO, 1u << BOARD_LED_GREEN_GPIO_PIN);
    GPIO_PortSet(BOARD_LED_BLUE_GPIO, 1u << BOARD_LED_BLUE_GPIO_PIN);

    PRINTF("Task_0\n\r");

    rtosDelay(10);
  }
}

void task1(void) // LED Green
{
  while (1)
  {
    GPIO_PortClear(BOARD_LED_GREEN_GPIO, 1u << BOARD_LED_GREEN_GPIO_PIN);
    GPIO_PortSet(BOARD_LED_RED_GPIO, 1u << BOARD_LED_RED_GPIO_PIN);
    GPIO_PortSet(BOARD_LED_BLUE_GPIO, 1u << BOARD_LED_BLUE_GPIO_PIN);

    PRINTF("Task_1\n\r");

    rtosDelay(30);
  }
}

void task2(void) // LED Blue
{
  while (1)
  {
    GPIO_PortClear(BOARD_LED_BLUE_GPIO, 1u << BOARD_LED_BLUE_GPIO_PIN);
    GPIO_PortSet(BOARD_LED_RED_GPIO, 1u << BOARD_LED_RED_GPIO_PIN);
    GPIO_PortSet(BOARD_LED_GREEN_GPIO, 1u << BOARD_LED_GREEN_GPIO_PIN);

    PRINTF("Task_2\n\r");

    rtosDelay(20);
  }
}

void taskIdel(void)
{
  for (;;)
    ;
}

void sortPriorities(void)
{
	uint8_t size = task_list.nTask + 1;
    /* Sort priorities to chose the next task to execute*/
  for(uint8_t i = 0; i < size; i++)
  {
    priorArray[i] = task_list.tasks[i].priority;
  }

  for (uint8_t i = 0; i < size; i++)
  {
    uint8_t j = i;
    while(j >= 0 && task_list.tasks[j].priority < task_list.tasks[j-1].priority)
    {
      uint8_t temp = priorArray[j];
      priorArray[j] = priorArray[j-1];
      priorArray[j-1] = temp;
      j--;
    }  
  }
}
void rtosStart(void)
{
  task_list.global_tick = 0;
  task_list.current_task = -1;

  SysTick->CTRL =
  SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk
    | SysTick_CTRL_ENABLE_Msk;
  rtosReloadSysTick();
  for (;;)
    ;
}

void rtosReloadSysTick(void)
{
  SysTick->LOAD = (SystemCoreClock / 1e3);
  SysTick->VAL = 0;
}

void rtosDelay(uint64_t ticks)
{
  task_list.tasks[task_list.current_task].state = stateWaiting;
  task_list.tasks[task_list.current_task].local_tick = ticks;
  rtosKernel(from_execution);
}

void rtosActivateWaitingTask(void)
{
  uint8_t idx;
  for (idx = 0; idx < task_list.nTask; idx++)
  {
    if (stateWaiting == task_list.tasks[idx].state)
    {
      task_list.tasks[idx].local_tick--;
      if (0 == task_list.tasks[idx].local_tick)
      {
        task_list.tasks[idx].state = stateReady;
      }
    }
  }
}

void rtosKernel(rtosContextSwitchFrom_t from)
{
  uint8_t nextTask = task_list.nTask;
  uint8_t findNextTask = 0;
  uint8_t foundNextTask = 0;
  uint8_t currentPriority = 0;

  static uint8_t first = 1;
  register uint32_t r0 asm("r0");

  (void) r0;

  /* calendarizador */
  do
  {
   if(findNextTask > task_list.nTask)
   {
	   findNextTask = 0;
	   currentPriority ++;
   }
   if(priorArray[currentPriority] == task_list.tasks[findNextTask].priority)
   {
	   if (stateReady == task_list.tasks[findNextTask].state
		   || stateRunning == task_list.tasks[findNextTask].state)
	   {
		   nextTask = findNextTask;
		   foundNextTask = 1;
	   }
	   else if (findNextTask == task_list.current_task)
	   {
		   foundNextTask = 1;
	   }
   }
   findNextTask++;
  } while (!foundNextTask);

  task_list.next_task = nextTask;

  /* context swtich */
  if (task_list.current_task != task_list.next_task)
  { // Context switching needed
    if (!first)
    {
      asm("mov r0, r7");
      task_list.tasks[task_list.current_task].sp = (uint32_t*) r0;
      if (from)
      {
        task_list.tasks[task_list.current_task].sp -= (-7);
        task_list.tasks[task_list.current_task].state = stateReady;
      }
      else
      {
        task_list.tasks[task_list.current_task].sp -= (9);
      }
    }
    else
    {
      first = 0;
    }

    task_list.current_task = task_list.next_task;
    task_list.tasks[task_list.current_task].state = stateRunning;
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // Set PendSV to pending
  }
}

void SysTick_Handler(void) // 1KHz
{
// Increment systick counter for LED blinking
  task_list.global_tick++;

  rtosActivateWaitingTask();

  rtosReloadSysTick();

  rtosKernel(from_isr);
}

void PendSV_Handler(void) // Context switching code
{
  register int32_t r0 asm("r0");
  (void) r0;
  SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
  r0 = (int32_t) task_list.tasks[task_list.current_task].sp;
  asm("mov r7,r0");
}
