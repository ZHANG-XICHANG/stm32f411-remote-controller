#ifndef LCD_TEST_HAL_H
#define LCD_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
/* Skip device-specific main.h while using the real LCD public header. */
#define __MAIN_H
typedef enum { RESET = 0, SET = 1 } FlagStatus;
typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int instance; } I2C_HandleTypeDef;
void HAL_Delay(uint32_t ms);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *bus, uint16_t address,
                                        uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *bus, uint16_t address,
                                      uint32_t trials, uint32_t timeout);
#endif
