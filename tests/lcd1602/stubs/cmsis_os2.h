#ifndef LCD_TEST_OS_H
#define LCD_TEST_OS_H
#include <stdint.h>
enum { osKernelInactive, osKernelRunning };
int osKernelGetState(void);
uint32_t osKernelGetTickFreq(void);
int osDelay(uint32_t ticks);
#endif
