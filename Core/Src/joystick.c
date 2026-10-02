#include "joystick.h"
#include "adc.h"

#define JOYSTICK_FILTER_ALPHA   0.8f
#define JOYSTICK_DEADZONE       50U

/* 第一版先使用理想 ADC 範圍，之後換成實際校正值 */
#define THR_MIN     0U
#define THR_MAX     4084U

#define THR_OUT_MIN  0U
#define THR_OUT_MAX  550U

#define YAW_MIN     0U
#define YAW_CENTER  2048U
#define YAW_MAX     4084U

#define PITCH_MIN     0U
#define PITCH_CENTER  2048U
#define PITCH_MAX     4084U

#define ROLL_MIN     0U
#define ROLL_CENTER  2048U
#define ROLL_MAX     4084U


JoystickRawData_t joystickRawData;
JoystickData_t joystickData;


/* 濾波後的 ADC 值，只在 joystick.c 內使用 */
static float filteredThr   = 0.0f;
static float filteredYaw   = 0.0f;
static float filteredPitch = 0.0f;
static float filteredRoll  = 0.0f;

static uint8_t filterInitialized = 0;


/**
 * @brief 讀取 ADC DMA 最新資料
 */
void Joystick_ReadData(void)
{
    joystickRawData.thr   = adc_raw[0];
    joystickRawData.yaw   = adc_raw[1];
    joystickRawData.pitch = adc_raw[2];
    joystickRawData.roll  = adc_raw[3];
}

static float Joystick_LowPassFilter(float input,
                                    float previous)
{
    return JOYSTICK_FILTER_ALPHA * previous
         + (1.0f - JOYSTICK_FILTER_ALPHA) * input;
}

/* 將 ADC 映射至 0～1000，中心死區固定輸出 500。 */
static uint16_t Joystick_MapCentered(uint16_t value,
                                    uint16_t min,
                                    uint16_t center,
                                    uint16_t max,
                                    uint16_t deadzone)
{
    int32_t result;

    /* 限制 ADC 範圍 */
    if (value < min)
    {
        value = min;
    }

    if (value > max)
    {
        value = max;
    }

    /* 中心死區 */
    if ((value >= (center - deadzone)) &&
        (value <= (center + deadzone)))
    {
        return 500;
    }

    /* 中心死區上緣至最大值：500～1000。 */
    if (value > center + deadzone)
    {
        result =
            500 + ((int32_t)value - (center + deadzone))
            * 500
            / ((int32_t)max - (center + deadzone));

        if (result > 1000)
        {
            result = 1000;
        }

        return (uint16_t)result;
    }

    /* 最小值至中心死區下緣：0～500。 */
    result =
        ((int32_t)value - min)
        * 500
        / ((int32_t)(center - deadzone) - min);

    if (result < 0)
    {
        result = 0;
    }

    return (uint16_t)result;
}

static uint16_t Joystick_MapThrottle(uint16_t value,
                                     uint16_t min,
                                     uint16_t max)
{
    if (value < min)
    {
        value = min;
    }

    if (value > max)
    {
        value = max;
    }

    return (uint16_t)(
        THR_OUT_MAX
        - ((uint32_t)(value - min) * (THR_OUT_MAX - THR_OUT_MIN))
          / (max - min)
    );
}

void Joystick_Process(void)
{
    /*
     * 第一次執行時直接使用目前 ADC 值初始化濾波器，
     * 避免從 0 慢慢爬到實際值。
     */
    if (!filterInitialized)
    {
        filteredThr   = joystickRawData.thr;
        filteredYaw   = joystickRawData.yaw;
        filteredPitch = joystickRawData.pitch;
        filteredRoll  = joystickRawData.roll;

        filterInitialized = 1;
    }
    else
    {
        filteredThr = Joystick_LowPassFilter(
            joystickRawData.thr,
            filteredThr
        );

        filteredYaw = Joystick_LowPassFilter(
            joystickRawData.yaw,
            filteredYaw
        );

        filteredPitch = Joystick_LowPassFilter(
            joystickRawData.pitch,
            filteredPitch
        );

        filteredRoll = Joystick_LowPassFilter(
            joystickRawData.roll,
            filteredRoll
        );
    }

    /*
     * 濾波完成後再做映射
     */

    joystickData.thr = Joystick_MapThrottle(
        (uint16_t)filteredThr,
        THR_MIN,
        THR_MAX
    );

    joystickData.yaw = Joystick_MapCentered(
        (uint16_t)filteredYaw,
        YAW_MIN,
        YAW_CENTER,
        YAW_MAX,
        JOYSTICK_DEADZONE
    );

    joystickData.pitch = Joystick_MapCentered(
        (uint16_t)filteredPitch,
        PITCH_MIN,
        PITCH_CENTER,
        PITCH_MAX,
        JOYSTICK_DEADZONE
    );

    joystickData.roll = Joystick_MapCentered(
        (uint16_t)filteredRoll,
        ROLL_MIN,
        ROLL_CENTER,
        ROLL_MAX,
        JOYSTICK_DEADZONE
    );
}
