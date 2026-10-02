#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

/* =========================
 * Joystick configuration
 * ========================= */

/* Low-pass filter coefficient */
#define JOYSTICK_FILTER_ALPHA   0.8f

/* Center dead zone in ADC counts */
#define JOYSTICK_DEADZONE       50U


/* =========================
 * ADC calibration values
 * ========================= */

/* Throttle */
#define THR_MIN     0U
#define THR_MAX     4084U

/* Yaw */
#define YAW_MIN     0U
#define YAW_CENTER  2048U
#define YAW_MAX     4084U

/* Pitch */
#define PITCH_MIN     0U
#define PITCH_CENTER  2048U
#define PITCH_MAX     4084U

/* Roll */
#define ROLL_MIN     0U
#define ROLL_CENTER  2048U
#define ROLL_MAX     4084U


/* =========================
 * Data structures
 * ========================= */

/**
 * @brief Raw ADC joystick data
 *
 * Range: 0 ~ 4095
 */
typedef struct
{
    uint16_t thr;
    uint16_t yaw;
    uint16_t pitch;
    uint16_t roll;

} JoystickRawData_t;


/**
 * @brief Processed joystick data
 *
 * thr:
 *   0 ~ 1000
 *
 * yaw / pitch / roll:
 *   0 ~ 1000 (center dead zone: 500)
 */
typedef struct
{
    uint16_t thr;

    uint16_t yaw;
    uint16_t pitch;
    uint16_t roll;

} JoystickData_t;


/* =========================
 * Global data
 * ========================= */

extern JoystickRawData_t joystickRawData;
extern JoystickData_t joystickData;


/* =========================
 * Public functions
 * ========================= */

/**
 * @brief Read latest ADC DMA values
 */
void Joystick_ReadData(void);


/**
 * @brief Filter and map joystick values
 *
 * Processing:
 * Raw ADC
 * → Low-pass filter
 * → Dead zone
 * → Mapping
 */
void Joystick_Process(void);


#endif /* JOYSTICK_H */
