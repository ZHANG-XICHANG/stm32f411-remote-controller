/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "pid_command.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <string.h>
#include "debug.h"
#include "joystick.h"
#include "button.h"
#include "nRF24L01P.h"
#include "ack_payload.h"
#include "transmit.h"
#include "connection.h"
#include "process_data.h"
#include "i2c.h"
#include "lcd1602_i2c.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* 除錯器可觀察：ButtonTask 歷來最少剩餘堆疊，單位為 StackType_t（4 bytes）。 */
volatile UBaseType_t buttonStackHighWaterMark = 0;
/* 溢位時保留任務資訊；名稱指向的記憶體也可能已遭破壞。 */
TaskHandle_t volatile stackOverflowTask = NULL;
char * volatile stackOverflowTaskName = NULL;

/* USER CODE END Variables */
/* Definitions for CommunicationTa */
osThreadId_t CommunicationTaHandle;
const osThreadAttr_t CommunicationTa_attributes = {
  .name = "CommunicationTa",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for JoystickTask */
osThreadId_t JoystickTaskHandle;
const osThreadAttr_t JoystickTask_attributes = {
  .name = "JoystickTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for ButtonTask */
osThreadId_t ButtonTaskHandle;
const osThreadAttr_t ButtonTask_attributes = {
  .name = "ButtonTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DisplayTask */
osThreadId_t DisplayTaskHandle;
const osThreadAttr_t DisplayTask_attributes = {
  .name = "DisplayTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for USBDebugTask */
osThreadId_t USBDebugTaskHandle;
const osThreadAttr_t USBDebugTask_attributes = {
  .name = "USBDebugTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for DebugQueue */
osMessageQueueId_t DebugQueueHandle;
const osMessageQueueAttr_t DebugQueue_attributes = {
  .name = "DebugQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartCommunicationTask(void *argument);
void StartJoystickTask(void *argument);
void StartButtonTask(void *argument);
void StartDisplayTask(void *argument);
void StartUSBDebugTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
   __disable_irq();
   stackOverflowTask = xTask;
   stackOverflowTaskName = (char *)pcTaskName;
   /* 在此設中斷點；堆疊已不可靠，不再呼叫 Debug_Printf。 */
   for (;;) {}
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
   __disable_irq();
   /* 配置失敗時停住，避免後續使用未成功建立的 RTOS 物件。 */
   for (;;) {}
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of DebugQueue */
  DebugQueueHandle = osMessageQueueNew (8, sizeof(DebugMessage_t), &DebugQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of CommunicationTa */
  CommunicationTaHandle = osThreadNew(StartCommunicationTask, NULL, &CommunicationTa_attributes);

  /* creation of JoystickTask */
  JoystickTaskHandle = osThreadNew(StartJoystickTask, NULL, &JoystickTask_attributes);

  /* creation of ButtonTask */
  ButtonTaskHandle = osThreadNew(StartButtonTask, NULL, &ButtonTask_attributes);

  /* creation of DisplayTask */
  DisplayTaskHandle = osThreadNew(StartDisplayTask, NULL, &DisplayTask_attributes);

  /* creation of USBDebugTask */
  USBDebugTaskHandle = osThreadNew(StartUSBDebugTask, NULL, &USBDebugTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartCommunicationTask */
/**
  * @brief  Function implementing the CommunicationTa thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartCommunicationTask */
void StartCommunicationTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartCommunicationTask */
  /* Infinite loop */
  TickType_t lastWakeTime;
  uint8_t radioReady = 0U;
  uint8_t txStatus;

  vTaskDelay(pdMS_TO_TICKS(1000));//等待 nRF24L01+ 上電完成

  Connection_Init();//初始化連線狀態
  lastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    if (!radioReady)
    {
      if (L01_Init() != HAL_OK || L01_Check() != HAL_OK)
      {
        Debug_Print("nRF24 init/check failed; retry in 100 ms\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        lastWakeTime = xTaskGetTickCount();
        continue;
      }
      radioReady = 1U;
      lastWakeTime = xTaskGetTickCount();
    }
    PidCommand_Poll();
    if (!PidCommand_Send(&txStatus)) txStatus = TransmitData();

    if (txStatus == L01_TX_SUCCESS)
    {
        Connection_ReportSuccess();
    }
    /* TX_DS 只表示收到 ACK；ACK 資料另存在 RX FIFO，須確實讀出。
     * 即使 TX 逾時也排空 FIFO，避免漏掉已到達的資料。
     */
    if (txStatus == L01_TX_SPI_ERROR || AckPayload_Poll() != HAL_OK)
    {
        radioReady = 0U;
        Debug_Print("nRF24 SPI failed; retry init in 100 ms\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        lastWakeTime = xTaskGetTickCount();
        continue;
    }

    vTaskDelayUntil(
      &lastWakeTime, 
      pdMS_TO_TICKS(6)
    );
  }
  /* USER CODE END StartCommunicationTask */
}

/* USER CODE BEGIN Header_StartJoystickTask */
/**
* @brief Function implementing the JoystickTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartJoystickTask */
void StartJoystickTask(void *argument)
{
  /* USER CODE BEGIN StartJoystickTask */
  /* Infinite loop */
    TickType_t lastWakeTime = xTaskGetTickCount();

    //static uint32_t rawDebugCounter = 0;
    static uint32_t processedDebugCounter = 0;

  for(;;)
  {
        /* 取得 DMA 最新 ADC */
        Joystick_ReadData();

        /* Filter + Deadzone + Mapping */
        Joystick_Process();
        ProcessJoystickData();

        /* 每 20 次看一次處理後結果 = 約 8.33 Hz */
        Debug_PrintfEveryN(
            &processedDebugCounter,
            20,
            "THR=%u YAW=%d PIT=%d ROL=%d RawTHR=%u RawYAW=%u RawPIT=%u RawROL=%u\r\n",
            joystickData.thr,
            joystickData.yaw,
            joystickData.pitch,
            joystickData.roll,
            joystickRawData.thr,
            joystickRawData.yaw,
            joystickRawData.pitch,
            joystickRawData.roll
        );

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(6)
        );
  }
  /* USER CODE END StartJoystickTask */
}

/* USER CODE BEGIN Header_StartButtonTask */
/**
* @brief Function implementing the ButtonTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartButtonTask */
void StartButtonTask(void *argument)
{
  /* USER CODE BEGIN StartButtonTask */
  /* Infinite loop */
  TickType_t lastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    ButtonEvent_t event = Button_Read();
    ProcessButtonData(event);

    if (event.pressed || event.released)
    {
        Debug_Printf(
            "Button pressed: 0x%02X, held: 0x%02X, released: 0x%02X\r\n",
            (unsigned int)event.pressed,
            (unsigned int)event.held,
            (unsigned int)event.released
        );
    }

    buttonStackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);

    vTaskDelayUntil(
        &lastWakeTime,
        pdMS_TO_TICKS(10)
    );
  }
  /* USER CODE END StartButtonTask */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
* @brief Function implementing the DisplayTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  static lcd1602_HandleTypeDef lcd = {0};

  ConnectionState_t lastState = (ConnectionState_t)0xFF;
  ConnectionState_t state;

  /* 等待 LCD 上電完成。 */
  vTaskDelay(pdMS_TO_TICKS(1500)); 

  /*初始化 LCD */
  if (lcd1602_Init(&lcd, &hi2c1, PCF8574_ADDRESS) == HAL_OK)
  {
      lcd1602_Print(&lcd, (const uint8_t *)"Hello");
  }
  else
  {
      /* LCD 初始化失敗，直接結束此 Task */
      vTaskDelete(NULL);
  }
  /* Infinite loop */
  TickType_t lastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    state = Connection_GetState();
    
    /* 只有狀態改變時才更新 LCD */
    if (state != lastState)
    {
        lastState = state;

        /* 第二行 */
        lcd1602_SetCursor(&lcd, 0, 1);

        if (state == CONNECTION_DISCONNECTED)
        {
          lcd1602_Print(
                &lcd,
                (const uint8_t *)"Disconnected    "
            );
        }
        else if (state == CONNECTION_CONNECTED)
        {
            lcd1602_Print(
                &lcd,
                (const uint8_t *)"Connected       "
            );
        }
        else
        {
            lcd1602_Print(
                &lcd,
                (const uint8_t *)"Unknown         "
            );
        }
    }
    
    vTaskDelayUntil(
        &lastWakeTime,
        pdMS_TO_TICKS(50)
    );
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartUSBDebugTask */
/**
* @brief Function implementing the USBDebugTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUSBDebugTask */
void StartUSBDebugTask(void *argument)
{
  /* USER CODE BEGIN StartUSBDebugTask */
  /* Infinite loop */

  
  for(;;)
  {
    Debug_Task();
    osDelay(1);
  }
  /* USER CODE END StartUSBDebugTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

