#ifndef LCD1602_I2C_H
#define LCD1602_I2C_H

#include "main.h"
#include <stdbool.h>

/* 已左移一位的 HAL 位址；實際位址由背板 A0～A2 決定，不要再次左移。 */
#define PCF8574_ADDRESS  (0x27U << 1)
#define PCF8574A_ADDRESS (0x3FU << 1)
#define LCD1602_I2C_TIMEOUT_MS 20U
#define LCD1602_COLUMNS 16U
#define LCD1602_ROWS 2U

typedef struct
{
    FlagStatus RS_Pin;
    FlagStatus RW_Pin;
    FlagStatus E_Pin;
    FunctionalState LED;
} ctrlPins_TypeDef;

typedef struct
{
    FunctionalState displayState;
    FunctionalState cursorState;
    FunctionalState blinkState;
} dispBits_TypeDef;

typedef enum { ShiftRight, ShiftLeft } ShiftDirection;

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint8_t address;
    uint8_t instruction;
    uint8_t data;
    ctrlPins_TypeDef ctrlPins;
    dispBits_TypeDef dispBits;
    uint8_t output;   /* 最近一次成功寫入的完整 PCF8574 輸出。 */
    bool initialized;
} lcd1602_HandleTypeDef;

/*
 * HD44780 相容 16x2 LCD + PCF8574 背板，I2C 使用 100 kHz：
 * P0=RS、P1=RW、P2=E、P3=背光（高有效）、P4～P7=D4～D7。
 * Handle 請先以 {0} 初始化。由單一 Task 操作，或由呼叫端鎖定整個
 * LCD 操作序列；不可從 ISR／臨界區呼叫，其他 I2C 使用者也須協調存取。
 * 排程器運行時延遲使用 osDelay，啟動前使用 HAL_Delay。
 *
 * 所有 API 回傳 HAL_StatusTypeDef。參數無效／未初始化時為 HAL_ERROR；
 * I2C 失敗回傳 HAL 原始錯誤，並使 initialized=false，須重新 Init。
 */

/* 上電等待、位址 ACK 檢查、4-bit 初始化；HAL_OK 不代表已驗證可見字元。 */
HAL_StatusTypeDef lcd1602_Init(lcd1602_HandleTypeDef *lcd, I2C_HandleTypeDef *hi2c, uint8_t address);

/* NUL 結尾字串；空字串不傳送資料。不自動換行，超過可見區會寫入隱藏 DDRAM。 */
HAL_StatusTypeDef lcd1602_Print(lcd1602_HandleTypeDef *lcd, const uint8_t *text);

/* col=0～15，row=0～1，超出範圍回傳 HAL_ERROR。 */
HAL_StatusTypeDef lcd1602_SetCursor(lcd1602_HandleTypeDef *lcd, uint8_t col, uint8_t row);
HAL_StatusTypeDef lcd1602_Clear(lcd1602_HandleTypeDef *lcd);
HAL_StatusTypeDef lcd1602_Home(lcd1602_HandleTypeDef *lcd);
HAL_StatusTypeDef lcd1602_Display(lcd1602_HandleTypeDef *lcd, FunctionalState state);
HAL_StatusTypeDef lcd1602_Cursor(lcd1602_HandleTypeDef *lcd, FunctionalState state);
HAL_StatusTypeDef lcd1602_Blink(lcd1602_HandleTypeDef *lcd, FunctionalState state);
HAL_StatusTypeDef lcd1602_LED(lcd1602_HandleTypeDef *lcd, FunctionalState state);
HAL_StatusTypeDef lcd1602_DisplayShift(lcd1602_HandleTypeDef *lcd, ShiftDirection direction);
HAL_StatusTypeDef lcd1602_CursorShift(lcd1602_HandleTypeDef *lcd, ShiftDirection direction);

#endif /* LCD1602_I2C_H */
