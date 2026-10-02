#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lcd1602_i2c.h"
#include "cmsis_os2.h"
#include "debug.h"

/* 主機測試不使用 USB 除錯佇列。 */
void Debug_Print(const char *text) { (void)text; }
void Debug_Printf(const char *format, ...) { (void)format; }

static I2C_HandleTypeDef bus;
static struct { uint8_t data[3]; uint16_t size; uint32_t time; } writes[64];
static unsigned int count, attempts, probeCalls, failAt, osWaits, halWaits;
static HAL_StatusTypeDef probeStatus, transferError;
static uint32_t now, tickFreq = 1000;
static int kernelState = osKernelRunning;
static uint16_t expectedAddress = PCF8574_ADDRESS;

int osKernelGetState(void) { return kernelState; }
uint32_t osKernelGetTickFreq(void) { return tickFreq; }
int osDelay(uint32_t ticks) { osWaits++; now += ticks * 1000U / tickFreq; return 0; }
void HAL_Delay(uint32_t ms) { halWaits++; now += ms + 1U; }

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *i2c, uint16_t address,
                                      uint32_t trials, uint32_t timeout)
{
    assert(i2c == &bus && address == expectedAddress);
    assert(trials > 0 && timeout == LCD1602_I2C_TIMEOUT_MS);
    assert(now >= 50U);
    probeCalls++;
    return probeStatus;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *i2c, uint16_t address,
                                        uint8_t *data, uint16_t size, uint32_t timeout)
{
    assert(i2c == &bus && address == expectedAddress);
    assert(timeout == LCD1602_I2C_TIMEOUT_MS);
    attempts++;
    if (attempts == failAt) return transferError;
    assert(count < sizeof(writes) / sizeof(writes[0]));
    assert(size == 1 || size == 3);
    if (size == 3)
    {
        /* Data/RS/RW/backlight must not change on either E edge. */
        assert((data[0] & 0x06U) == 0U);
        assert((data[0] ^ data[1]) == 0x04U);
        assert(data[0] == data[2]);
    }
    memcpy(writes[count].data, data, size);
    writes[count].size = size;
    writes[count++].time = now;
    return HAL_OK;
}

static void resetTrace(void)
{
    count = attempts = probeCalls = failAt = osWaits = halWaits = 0;
    now = 0;
    probeStatus = HAL_OK;
    transferError = HAL_TIMEOUT;
}

static uint8_t decoded(unsigned int index, int isData)
{
    assert(index + 1U < count);
    assert(writes[index].size == 3 && writes[index + 1U].size == 3);
    assert((writes[index].data[0] & 1U) == (unsigned)isData);
    assert((writes[index + 1U].data[0] & 1U) == (unsigned)isData);
    return (uint8_t)((writes[index].data[0] & 0xF0U) | (writes[index + 1U].data[0] >> 4));
}

static void checkInit(lcd1602_HandleTypeDef *lcd)
{
    resetTrace();
    assert(lcd1602_Init(lcd, &bus, (uint8_t)expectedAddress) == HAL_OK);
    assert(lcd->initialized && count == 16 && probeCalls == 1);
    assert(writes[0].data[0] == 0 && writes[0].time >= 50U);
    const uint8_t nibbles[] = {0x30, 0x30, 0x30, 0x20};
    for (unsigned int i = 0; i < 4; i++) assert(writes[i + 1].data[0] == nibbles[i]);
    assert(writes[2].time - writes[1].time >= 5U);
    assert(writes[3].time - writes[2].time >= 1U);
    const uint8_t commands[] = {0x28, 0x08, 0x01, 0x06, 0x0C};
    for (unsigned int i = 0; i < sizeof(commands); i++) assert(decoded(5U + i * 2U, 0) == commands[i]);
    assert(writes[11].time - writes[10].time >= 3U); /* Clear execution delay. */
    assert(lcd->output == 0xC8 && lcd->ctrlPins.LED == ENABLE);
    assert(lcd->dispBits.displayState == ENABLE && lcd->dispBits.cursorState == DISABLE);
    assert(kernelState == osKernelRunning ? (osWaits && !halWaits) : (halWaits && !osWaits));
}

int main(void)
{
    lcd1602_HandleTypeDef lcd = {0};
    resetTrace();
    assert(lcd1602_Print(&lcd, (const uint8_t *)"x") == HAL_ERROR);
    assert(lcd1602_Init(NULL, &bus, PCF8574_ADDRESS) == HAL_ERROR);
    assert(lcd1602_Init(&lcd, NULL, PCF8574_ADDRESS) == HAL_ERROR);
    assert(lcd1602_Init(&lcd, &bus, 0x27) == HAL_ERROR);
    assert(attempts == 0 && probeCalls == 0);
    checkInit(&lcd);

    resetTrace();
    assert(lcd1602_Print(&lcd, (const uint8_t *)"") == HAL_OK && count == 0);
    assert(lcd1602_Print(&lcd, NULL) == HAL_ERROR);
    assert(lcd1602_SetCursor(&lcd, 16, 0) == HAL_ERROR);
    assert(lcd1602_SetCursor(&lcd, 0, 2) == HAL_ERROR);
    assert(lcd1602_Display(&lcd, (FunctionalState)2) == HAL_ERROR);
    assert(lcd1602_DisplayShift(&lcd, (ShiftDirection)2) == HAL_ERROR);
    assert(count == 0 && lcd.initialized);
    assert(lcd1602_SetCursor(&lcd, 0, 0) == HAL_OK && decoded(0, 0) == 0x80);
    assert(lcd1602_SetCursor(&lcd, 15, 1) == HAL_OK && decoded(2, 0) == 0xCF);

    /* Exhaust all non-NUL byte values to verify nibble order and RS. */
    for (unsigned int ch = 1; ch <= 255; ch++)
    {
        uint8_t text[] = {(uint8_t)ch, 0};
        resetTrace();
        assert(lcd1602_Print(&lcd, text) == HAL_OK && count == 2);
        assert(decoded(0, 1) == ch);
        assert(lcd.data == ch && (lcd.output & 0x0CU) == 0x08U);
    }

    resetTrace();
    uint8_t oldOutput = lcd.output;
    assert(lcd1602_LED(&lcd, DISABLE) == HAL_OK);
    assert(lcd.output == (oldOutput & 0xF7U));
    assert(lcd1602_LED(&lcd, ENABLE) == HAL_OK && lcd.output == oldOutput);
    assert(lcd1602_Cursor(&lcd, ENABLE) == HAL_OK && decoded(2, 0) == 0x0E);
    assert(lcd1602_Blink(&lcd, ENABLE) == HAL_OK && decoded(4, 0) == 0x0F);
    assert(lcd1602_Display(&lcd, DISABLE) == HAL_OK && decoded(6, 0) == 0x0B);
    assert(lcd1602_Cursor(&lcd, DISABLE) == HAL_OK && decoded(8, 0) == 0x09);
    assert(lcd1602_DisplayShift(&lcd, ShiftLeft) == HAL_OK && decoded(10, 0) == 0x18);
    assert(lcd1602_DisplayShift(&lcd, ShiftRight) == HAL_OK && decoded(12, 0) == 0x1C);
    assert(lcd1602_CursorShift(&lcd, ShiftLeft) == HAL_OK && decoded(14, 0) == 0x10);
    assert(lcd1602_CursorShift(&lcd, ShiftRight) == HAL_OK && decoded(16, 0) == 0x14);
    uint32_t before = now;
    assert(lcd1602_Home(&lcd) == HAL_OK && decoded(18, 0) == 0x02 && now - before >= 3U);

    /* A failed probe or any failed initialization transfer must stop immediately. */
    resetTrace();
    probeStatus = HAL_ERROR;
    assert(lcd1602_Init(&lcd, &bus, PCF8574_ADDRESS) == HAL_ERROR);
    assert(!lcd.initialized && attempts == 0);
    for (unsigned int i = 1; i <= 16; i++)
    {
        resetTrace();
        failAt = i;
        assert(lcd1602_Init(&lcd, &bus, PCF8574_ADDRESS) == HAL_TIMEOUT);
        assert(!lcd.initialized && attempts == i);
    }
    /* Fail either half-byte: no next character, no further command until re-init. */
    for (unsigned int i = 1; i <= 2; i++)
    {
        checkInit(&lcd);
        resetTrace();
        failAt = i;
        transferError = HAL_BUSY;
        assert(lcd1602_Print(&lcd, (const uint8_t *)"AB") == HAL_BUSY);
        assert(!lcd.initialized && attempts == i);
        assert(lcd1602_Clear(&lcd) == HAL_ERROR && attempts == i);
    }

    kernelState = osKernelInactive;
    expectedAddress = PCF8574A_ADDRESS;
    checkInit(&lcd);
    kernelState = osKernelRunning;
    tickFreq = 100;
    checkInit(&lcd);
    puts("PASS: init sequence, E/data stability, all character bytes, bounds, controls, ACK/errors, recovery, RTOS/HAL delays");
    return 0;
}
