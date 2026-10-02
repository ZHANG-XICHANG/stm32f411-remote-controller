#include "debug.h"

#include "cmsis_os2.h"
#include "usbd_cdc_if.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
 * DebugQueueHandle 由 CubeMX / FreeRTOS 建立。
 *
 * 實際名稱請確認你的 freertos.c。
 */
extern osMessageQueueId_t DebugQueueHandle;

/**
 * @brief 每累計 every_n 次有效呼叫，允許輸出一次 Debug 訊息。
 *
 * @param counter 呼叫端維護的計數器指標，初值應為 0，並保留至下次呼叫。
 * @param every_n 輸出間隔（呼叫次數）；1 表示每次輸出，0 表示停用。
 * @return 1：已達輸出間隔；0：尚未達到間隔或參數無效。
 * @note 此函式只更新計數並判斷是否輸出，不會實際傳送訊息。
 */
static uint8_t Debug_ShouldPrint(
    uint32_t *counter,
    uint32_t every_n
)
{
    /* 無計數器或停用輸出時直接返回，不更新計數。 */
    if (counter == NULL || every_n == 0)
    {
        return 0;
    }

    /* 累計本次呼叫；遞增的是指標指向的計數值。 */
    (*counter)++;

    /* 尚未累計到指定次數，本次略過輸出。 */
    if (*counter < every_n)
    {
        return 0;
    }

    /* 達到指定次數後歸零，開始下一輪計數，並允許本次輸出。 */
    *counter = 0;

    return 1;
}

/**
 * @brief 發送除錯字串到 Debug Queue
 */
void Debug_Print(const char *text)
{
    if (!DEBUG_PRINT_ENABLE)
    {
        return;
    }

    DebugMessage_t msg = {0}; // Initialize the message structure

    if (text == NULL) // 如果傳入的字串為 NULL，直接返回
    {
        return;
    }

    strncpy( //複製字串到 msg.text，確保不會超過 DEBUG_MSG_SIZE - 1
        msg.text,
        text,
        DEBUG_MSG_SIZE - 1
    );

    /*
     * timeout = 0
     *
     * Queue 滿就直接放棄 Debug，
     * 絕對不能因為 Debug 卡住控制 Task。
     */
    osMessageQueuePut(
        DebugQueueHandle,
        &msg,
        0,
        0
    );
}

/**
 * @brief 格式化並發送除錯訊息
 */
void Debug_Printf(const char *format, ...)
{
    if (!DEBUG_PRINT_ENABLE)
    {
        return;
    }

    DebugMessage_t msg = {0};

    if (format == NULL)
    {
        return;
    }

    va_list args;

    va_start(args, format);

    vsnprintf(
        msg.text,
        DEBUG_MSG_SIZE,
        format,
        args
    );

    va_end(args);

    osMessageQueuePut(
        DebugQueueHandle,
        &msg,
        0,
        0
    );
}

/**
 * @brief 每呼叫 N 次才輸出一次固定字串
 *
 * @param counter 呼叫端自己的計數器
 * @param every_n 每幾次輸出一次
 * @param text 要輸出的字串
 */
void Debug_PrintEveryN(
    uint32_t *counter,
    uint32_t every_n,
    const char *text
)
{
    if (!DEBUG_PRINT_EVERY_N_ENABLE)
    {
        return;
    }

    if (text == NULL)
    {
        return;
    }

    if (!Debug_ShouldPrint(counter, every_n))
    {
        return;
    }

    DebugMessage_t msg = {0};

    strncpy(
        msg.text,
        text,
        sizeof(msg.text) - 1
    );

    osMessageQueuePut(
        DebugQueueHandle,
        &msg,
        0,
        0
    );
}

/**
 * @brief 每呼叫 N 次才格式化並輸出一次
 *
 * @param counter 呼叫端自己的計數器
 * @param every_n 每幾次輸出一次
 * @param format printf 格式
 */
void Debug_PrintfEveryN(
    uint32_t *counter,
    uint32_t every_n,
    const char *format,
    ...
)
{
    if (!DEBUG_PRINT_EVERY_N_ENABLE)
    {
        return;
    }

    if (format == NULL)
    {
        return;
    }

    /*
     * 尚未到指定次數：
     * 直接返回，不執行 vsnprintf()
     */
    if (!Debug_ShouldPrint(counter, every_n))
    {
        return;
    }

    DebugMessage_t msg = {0};

    va_list args;

    va_start(args, format);

    vsnprintf(
        msg.text,
        sizeof(msg.text),
        format,
        args
    );

    va_end(args);

    /*
     * Debug Queue 滿就丟掉。
     * 絕不能因為 Debug 阻塞控制 Task。
     */
    osMessageQueuePut(
        DebugQueueHandle,
        &msg,
        0,
        0
    );
}
/**
 * @brief USB Debug Task 主循環
 */
void Debug_Task(void)
{
    DebugMessage_t msg;

    for (;;)
    {
        if (osMessageQueueGet(
                DebugQueueHandle,
                &msg,
                NULL,
                osWaitForever) == osOK)
        {
            uint8_t retry = 5;

            while (retry--)
            {
                if (CDC_Transmit_FS(
                        (uint8_t *)msg.text,
                        strlen(msg.text)) == USBD_OK)
                {
                    break;
                }

                osDelay(1);
            }
        }
    }
}
