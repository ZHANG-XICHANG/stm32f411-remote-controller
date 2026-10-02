#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

#define DEBUG_MSG_SIZE 128

/* 一般輸出開關：控制 Debug_Print / Debug_Printf，1 啟用、0 停用。 */
#ifndef DEBUG_PRINT_ENABLE
#define DEBUG_PRINT_ENABLE 0
#endif

/* 間隔輸出開關：控制 Debug_PrintEveryN / Debug_PrintfEveryN。
 * 1 啟用、0 停用；停用時不更新計數器。修改開關後需重新編譯。
 */
#ifndef DEBUG_PRINT_EVERY_N_ENABLE
#define DEBUG_PRINT_EVERY_N_ENABLE 0
#endif

typedef struct
{
    char text[DEBUG_MSG_SIZE];
} DebugMessage_t;

/**
 * @brief 發送除錯字串到 Debug Queue
 */
void Debug_Print(const char *text);

/**
 * @brief 格式化並發送除錯訊息
 */
void Debug_Printf(const char *format, ...);

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
);

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
);

/**
 * @brief USB Debug Task 主循環
 */
void Debug_Task(void);

#endif
