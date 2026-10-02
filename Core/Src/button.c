#include "button.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t mask;
} ButtonConfig_t;

/* 對應 CubeMX 已設定的六顆按鈕 GPIO。 */
static const ButtonConfig_t buttons[BUTTON_COUNT] = {
    {Button_1_GPIO_Port, Button_1_Pin, Button_1},
    {Button_2_GPIO_Port, Button_2_Pin, Button_2},
    {Button_3_GPIO_Port, Button_3_Pin, Button_3},
    {Button_4_GPIO_Port, Button_4_Pin, Button_4},
    {Button_5_GPIO_Port, Button_5_Pin, Button_5},
    {Button_6_GPIO_Port, Button_6_Pin, Button_6},
};

ButtonEvent_t Button_Read(void)
{
    /* 各按鈕獨立保存消抖計數與按下狀態，支援同時按鍵。 */
    static uint8_t press_count[BUTTON_COUNT] = {0};
    static uint8_t release_count[BUTTON_COUNT] = {0};
    static uint8_t pressed[BUTTON_COUNT] = {0};

    ButtonEvent_t event = {0};

    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        if (HAL_GPIO_ReadPin(buttons[i].port, buttons[i].pin) == GPIO_PIN_RESET)
        {
            /* 再次讀到低電位，取消尚未穩定的放開計數。 */
            release_count[i] = 0;

            if (!pressed[i])
            {
                if (press_count[i] < BUTTON_DEBOUNCE_COUNT)
                {
                    press_count[i]++;
                }

                if (press_count[i] >= BUTTON_DEBOUNCE_COUNT)
                {
                    pressed[i] = 1;
                    event.pressed |= buttons[i].mask;
                }
            }
            else
            {
                event.held |= buttons[i].mask;
            }
        }
        else
        {
            if (pressed[i])
            {
                if (release_count[i] < BUTTON_DEBOUNCE_COUNT)
                {
                    release_count[i]++;
                }

                if (release_count[i] >= BUTTON_DEBOUNCE_COUNT)
                {
                    /* 有效放開後清除狀態，準備下一次按下。 */
                    pressed[i] = 0;
                    press_count[i] = 0;
                    release_count[i] = 0;
                    event.released |= buttons[i].mask;
                }
            }
            else
            {
                /* 按下尚未穩定就回到高電位，重新累計。 */
                press_count[i] = 0;
            }
        }
    }

    return event;
}
