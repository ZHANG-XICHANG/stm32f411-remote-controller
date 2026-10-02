#ifndef PROCESS_DATA_H
#define PROCESS_DATA_H

#include "button.h"

/* 使用映射後的油門值（0～1000），嚴格小於此值才允許 power 事件。 */
#define POWER_THROTTLE_THRESHOLD 5U

/* 微調以映射後的數值為單位，RAM 內累積，重新啟動後歸零。 */
#define JOYSTICK_TRIM_STEP 10
#define JOYSTICK_TRIM_LIMIT 1000

/* 由搖桿 Task 在 Joystick_Process() 後呼叫，同步四軸到發送資料。 */
void ProcessJoystickData(void);

/*
 * 由按鈕 Task 傳入本次消抖事件：
 * Button_1/2 釋放：pitch 微調 +10/-10；Button_3/4 釋放：roll 微調 +10/-10。
 * 微調累積範圍 ±JOYSTICK_TRIM_LIMIT，四軸更新時套用，輸出限制在 0～1000。
 * Button_6 釋放且油門 <5：power 置 1，由 TransmitData() 取走後清零。
 */
void ProcessButtonData(ButtonEvent_t event);

#endif /* PROCESS_DATA_H */
