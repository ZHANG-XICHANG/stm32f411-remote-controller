#include "process_data.h"
#include "joystick.h"
#include "transmit.h"
#include "FreeRTOS.h"
#include "task.h"

/* 避免還沒取得搖桿資料時，把預設的 0 誤認為最低油門。 */
static uint8_t joystickDataReady = 0U;

/* 保存未微調的最近一次搖桿值，避免把微調重複加到上次輸出。 */
static uint16_t basePitch = 0U;
static uint16_t baseRoll = 0U;
static int16_t pitchTrim = 0;
static int16_t rollTrim = 0;

static int16_t ClampTrim(int32_t trim)
{
    if (trim > JOYSTICK_TRIM_LIMIT) return JOYSTICK_TRIM_LIMIT;
    if (trim < -JOYSTICK_TRIM_LIMIT) return -JOYSTICK_TRIM_LIMIT;
    return (int16_t)trim;
}

static uint16_t ApplyTrim(uint16_t value, int16_t trim)
{
    int32_t adjusted = (int32_t)value + trim;
    if (adjusted < 0) return 0U;
    if (adjusted > 1000) return 1000U;
    return (uint16_t)adjusted;
}

void ProcessJoystickData(void)
{
    taskENTER_CRITICAL();
    remoteData.thr = joystickData.thr;
    remoteData.yaw = joystickData.yaw;
    basePitch = joystickData.pitch;
    baseRoll = joystickData.roll;
    remoteData.pitch = ApplyTrim(basePitch, pitchTrim);
    remoteData.roll = ApplyTrim(baseRoll, rollTrim);
    joystickDataReady = 1U;
    taskEXIT_CRITICAL();
}

void ProcessButtonData(ButtonEvent_t event)
{
    if ((event.released & (Button_1 | Button_2 | Button_3 | Button_4 | Button_6)) == 0U)
    {
        return;
    }

    /* 以釋放事件當下最新的油門快照判斷，與發送端共用臨界區保護。 */
    taskENTER_CRITICAL();
    /* 同軸的加、減按鈕若同時釋放，淨變化為 0（邊界時也一致）。 */
    int32_t pitchDelta = ((event.released & Button_1) ? JOYSTICK_TRIM_STEP : 0)
                       - ((event.released & Button_2) ? JOYSTICK_TRIM_STEP : 0);
    int32_t rollDelta = ((event.released & Button_3) ? JOYSTICK_TRIM_STEP : 0)
                      - ((event.released & Button_4) ? JOYSTICK_TRIM_STEP : 0);
    pitchTrim = ClampTrim((int32_t)pitchTrim + pitchDelta);
    rollTrim = ClampTrim((int32_t)rollTrim + rollDelta);

    if (joystickDataReady)
    {
        remoteData.pitch = ApplyTrim(basePitch, pitchTrim);
        remoteData.roll = ApplyTrim(baseRoll, rollTrim);
    }

    if ((event.released & Button_6) != 0U &&
        joystickDataReady && remoteData.thr < POWER_THROTTLE_THRESHOLD)
    {
        remoteData.power = 1U;
    }
    /* 不在其他掃描清零，讓待發送事件保留到 TransmitData() 取走。 */
    taskEXIT_CRITICAL();
}
