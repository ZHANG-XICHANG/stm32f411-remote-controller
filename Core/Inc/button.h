#ifndef BUTTON_H
#define BUTTON_H

#include "main.h"

#define BUTTON_COUNT          6U
#define BUTTON_DEBOUNCE_COUNT 3U

/* 六顆按鈕各佔一個 bit，可同時表示多顆按鈕事件。 */
#define BUTTON_NONE 0x00U
#define Button_1    0x01U
#define Button_2    0x02U
#define Button_3    0x04U
#define Button_4    0x08U
#define Button_5    0x10U
#define Button_6    0x20U

typedef struct
{
    uint8_t pressed;  /* 消抖後剛按下，只回報一次。 */
    uint8_t held;     /* 確認按下後，持續讀到低電位時回報。 */
    uint8_t released; /* 消抖後剛放開，只回報一次。 */
} ButtonEvent_t;

/**
 * @brief 掃描 Button_1～Button_6，回傳本次按鈕事件。
 * @note 低電位表示按下；按下與放開皆需連續穩定三次掃描。
 *       請由單一 Task 固定週期呼叫，消抖時間取決於掃描週期。
 * @return 各欄位為 bit mask，例如 event.pressed & Button_1。
 */
ButtonEvent_t Button_Read(void);

#endif /* BUTTON_H */
