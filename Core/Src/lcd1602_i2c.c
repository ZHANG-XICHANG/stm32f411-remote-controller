#include "lcd1602_i2c.h"
#include "cmsis_os2.h"
#include "debug.h"

#define LCD_RS 0x01U
#define LCD_RW 0x02U
#define LCD_E  0x04U
#define LCD_BACKLIGHT 0x08U

static void LCD_DelayMs(uint32_t milliseconds)
{
    if (osKernelGetState() == osKernelRunning)
    {
        /* 多加一個 tick，避免 tick 邊界使等待短於指定時間。 */
        uint32_t ticks = (milliseconds * osKernelGetTickFreq() + 999U) / 1000U;
        (void)osDelay(ticks + 1U);
    }
    else
    {
        HAL_Delay(milliseconds);
    }
}

static bool LCD_Ready(const lcd1602_HandleTypeDef *lcd)
{
    return lcd != NULL && lcd->hi2c != NULL && lcd->initialized;
}

static bool LCD_ValidState(FunctionalState state)
{
    return state == ENABLE || state == DISABLE;
}

static HAL_StatusTypeDef LCD_Transfer(lcd1602_HandleTypeDef *lcd, uint8_t *bytes, uint16_t size)
{
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        lcd->hi2c, lcd->address, bytes, size, LCD1602_I2C_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        /* 部分資料可能已送出，不能假設 LCD 仍保持正確的 nibble 相位。 */
        lcd->initialized = false;
        return status;
    }
    lcd->output = bytes[size - 1U];
    lcd->ctrlPins.RS_Pin = (lcd->output & LCD_RS) ? SET : RESET;
    lcd->ctrlPins.RW_Pin = (lcd->output & LCD_RW) ? SET : RESET;
    lcd->ctrlPins.E_Pin = (lcd->output & LCD_E) ? SET : RESET;
    lcd->ctrlPins.LED = (lcd->output & LCD_BACKLIGHT) ? ENABLE : DISABLE;
    return HAL_OK;
}

static HAL_StatusTypeDef LCD_WriteNibble(lcd1602_HandleTypeDef *lcd, uint8_t value, bool data)
{
    uint8_t base = (value & 0xF0U) | (lcd->output & LCD_BACKLIGHT);
    if (data) base |= LCD_RS;
    /* 先設定資料，再令 E=1，最後令 E=0；三個 byte 的資料位元保持不變。
     * 100 kHz I2C 的 byte 間隔提供足夠的 setup、E pulse 與 hold 時間。 */
    uint8_t pulse[3] = {base, (uint8_t)(base | LCD_E), base};
    return LCD_Transfer(lcd, pulse, sizeof(pulse));
}

static HAL_StatusTypeDef LCD_WriteByte(lcd1602_HandleTypeDef *lcd, uint8_t value, bool data)
{
    HAL_StatusTypeDef status = LCD_WriteNibble(lcd, value, data);
    if (status != HAL_OK) return status;
    status = LCD_WriteNibble(lcd, (uint8_t)(value << 4), data);
    if (status != HAL_OK) return status;

    if (data) lcd->data = value;
    else lcd->instruction = value;
    /* 不讀 Busy Flag；Clear/Home 需要比一般指令更久的執行時間。 */
    LCD_DelayMs(!data && (value == 0x01U || value == 0x02U) ? 3U : 1U);
    return HAL_OK;
}

static HAL_StatusTypeDef LCD_DisplayControl(lcd1602_HandleTypeDef *lcd, dispBits_TypeDef bits)
{
    uint8_t command = 0x08U;
    if (bits.displayState == ENABLE) command |= 0x04U;
    if (bits.cursorState == ENABLE) command |= 0x02U;
    if (bits.blinkState == ENABLE) command |= 0x01U;
    HAL_StatusTypeDef status = LCD_WriteByte(lcd, command, false);
    if (status == HAL_OK) lcd->dispBits = bits;
    return status;
}

static HAL_StatusTypeDef LCD_Init(lcd1602_HandleTypeDef *lcd, I2C_HandleTypeDef *hi2c, uint8_t address)
{
    if (lcd == NULL) return HAL_ERROR;
    *lcd = (lcd1602_HandleTypeDef){0};
    /* HAL 使用左移後的 7-bit 位址，bit 0 必須為 0。 */
    if (hi2c == NULL || address == 0U || (address & 1U) != 0U) return HAL_ERROR;
    lcd->hi2c = hi2c;
    lcd->address = address;

    LCD_DelayMs(50U);
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(hi2c, address, 2U, LCD1602_I2C_TIMEOUT_MS);
    if (status != HAL_OK) return status;

    /* 先令 RS/RW/E 與背光為低，消除 PCF8574 上電預設的高電位。 */
    uint8_t idle = 0U;
    status = LCD_Transfer(lcd, &idle, 1U);
    if (status != HAL_OK) return status;
    LCD_DelayMs(3U);

    /* HD44780 重新同步：0x3、0x3、0x3、0x2，這裡各只送一個 nibble。 */
    const uint8_t resetNibbles[] = {0x30U, 0x30U, 0x30U, 0x20U};
    for (uint8_t i = 0U; i < sizeof(resetNibbles); i++)
    {
        status = LCD_WriteNibble(lcd, resetNibbles[i], false);
        if (status != HAL_OK) return status;
        LCD_DelayMs(i == 0U ? 5U : 1U);
    }

    /* 4-bit、2 行、5x8 字型；關閉顯示、清除、游標遞增、開啟顯示。 */
    const uint8_t commands[] = {0x28U, 0x08U, 0x01U, 0x06U, 0x0CU};
    for (uint8_t i = 0U; i < sizeof(commands); i++)
    {
        status = LCD_WriteByte(lcd, commands[i], false);
        if (status != HAL_OK) return status;
    }
    lcd->dispBits.displayState = ENABLE;
    uint8_t backlight = lcd->output | LCD_BACKLIGHT;
    status = LCD_Transfer(lcd, &backlight, 1U);
    if (status != HAL_OK) return status;
    lcd->initialized = true;
    return HAL_OK;
}

HAL_StatusTypeDef lcd1602_Init(lcd1602_HandleTypeDef *lcd, I2C_HandleTypeDef *hi2c, uint8_t address)
{
    /* 統一記錄結果，包含參數錯誤及初始化途中發生的 I2C 錯誤。 */
    HAL_StatusTypeDef status = LCD_Init(lcd, hi2c, address);
    if (status == HAL_OK)
    {
        Debug_Print("LCD1602 init OK\r\n");
    }
    else
    {
        Debug_Printf("LCD1602 init FAIL, status=%u\r\n", (unsigned int)status);
    }
    return status;
}

HAL_StatusTypeDef lcd1602_Print(lcd1602_HandleTypeDef *lcd, const uint8_t *text)
{
    if (!LCD_Ready(lcd) || text == NULL) return HAL_ERROR;
    while (*text != '\0')
    {
        HAL_StatusTypeDef status = LCD_WriteByte(lcd, *text++, true);
        if (status != HAL_OK) return status;
    }
    return HAL_OK;
}

HAL_StatusTypeDef lcd1602_SetCursor(lcd1602_HandleTypeDef *lcd, uint8_t col, uint8_t row)
{
    if (!LCD_Ready(lcd) || col >= LCD1602_COLUMNS || row >= LCD1602_ROWS) return HAL_ERROR;
    return LCD_WriteByte(lcd, (uint8_t)(0x80U | (col + 0x40U * row)), false);
}

HAL_StatusTypeDef lcd1602_Clear(lcd1602_HandleTypeDef *lcd)
{
    if (!LCD_Ready(lcd)) return HAL_ERROR;
    return LCD_WriteByte(lcd, 0x01U, false);
}

HAL_StatusTypeDef lcd1602_Home(lcd1602_HandleTypeDef *lcd)
{
    if (!LCD_Ready(lcd)) return HAL_ERROR;
    return LCD_WriteByte(lcd, 0x02U, false);
}

HAL_StatusTypeDef lcd1602_Display(lcd1602_HandleTypeDef *lcd, FunctionalState state)
{
    if (!LCD_Ready(lcd) || !LCD_ValidState(state)) return HAL_ERROR;
    dispBits_TypeDef bits = lcd->dispBits;
    bits.displayState = state;
    return LCD_DisplayControl(lcd, bits);
}

HAL_StatusTypeDef lcd1602_Cursor(lcd1602_HandleTypeDef *lcd, FunctionalState state)
{
    if (!LCD_Ready(lcd) || !LCD_ValidState(state)) return HAL_ERROR;
    dispBits_TypeDef bits = lcd->dispBits;
    bits.cursorState = state;
    return LCD_DisplayControl(lcd, bits);
}

HAL_StatusTypeDef lcd1602_Blink(lcd1602_HandleTypeDef *lcd, FunctionalState state)
{
    if (!LCD_Ready(lcd) || !LCD_ValidState(state)) return HAL_ERROR;
    dispBits_TypeDef bits = lcd->dispBits;
    bits.blinkState = state;
    return LCD_DisplayControl(lcd, bits);
}

HAL_StatusTypeDef lcd1602_LED(lcd1602_HandleTypeDef *lcd, FunctionalState state)
{
    if (!LCD_Ready(lcd) || !LCD_ValidState(state)) return HAL_ERROR;
    /* 保留 D4～D7、RS/RW，只更新背光位元，E 保持低。 */
    uint8_t output = (uint8_t)(lcd->output & ~(LCD_BACKLIGHT | LCD_E));
    if (state == ENABLE) output |= LCD_BACKLIGHT;
    return LCD_Transfer(lcd, &output, 1U);
}

HAL_StatusTypeDef lcd1602_DisplayShift(lcd1602_HandleTypeDef *lcd, ShiftDirection direction)
{
    if (!LCD_Ready(lcd) || (direction != ShiftRight && direction != ShiftLeft)) return HAL_ERROR;
    return LCD_WriteByte(lcd, direction == ShiftRight ? 0x1CU : 0x18U, false);
}

HAL_StatusTypeDef lcd1602_CursorShift(lcd1602_HandleTypeDef *lcd, ShiftDirection direction)
{
    if (!LCD_Ready(lcd) || (direction != ShiftRight && direction != ShiftLeft)) return HAL_ERROR;
    return LCD_WriteByte(lcd, direction == ShiftRight ? 0x14U : 0x10U, false);
}
