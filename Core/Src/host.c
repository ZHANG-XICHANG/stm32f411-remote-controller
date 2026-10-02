#include "host.h"
#include "debug.h"
#include "cmsis_os2.h"
#include <stdio.h>

extern osMessageQueueId_t DebugQueueHandle;

static uint8_t Host_QueueMessage(const DebugMessage_t *message, int length)
{
    if (length < 0 || (size_t)length >= sizeof(message->text) ||
        DebugQueueHandle == NULL)
    {
        return 0U;
    }
    return osMessageQueuePut(DebugQueueHandle, message, 0U, 0U) == osOK;
}

uint8_t Host_SendTelemetry(const TelemetryData_t *data)
{
    DebugMessage_t message = {0};
    if (data == NULL) return 0U;

    /* Integer wire units; serial_scope.py restores deg and deg/s. */
    int length = snprintf(message.text, sizeof(message.text),
                          "TEL,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u\r\n",
                          (unsigned long)data->sequence,
                          (unsigned long)data->time_ms,
                          (int)data->angle, (int)data->rate_target,
                          (int)data->rate, (int)data->rate_error,
                          (int)data->p_term, (int)data->d_term, (int)data->pid_output,
                          (unsigned int)data->esc_pwm[0], (unsigned int)data->esc_pwm[1],
                          (unsigned int)data->esc_pwm[2], (unsigned int)data->esc_pwm[3], (unsigned int)data->axis);
    return Host_QueueMessage(&message, length);
}

uint8_t Host_SendTelemetrySequence(uint32_t sequence)
{
    DebugMessage_t message = {0};
    int length = snprintf(message.text, sizeof(message.text),
                          "TEL_SEQ,%lu\r\n", (unsigned long)sequence);
    return Host_QueueMessage(&message, length);
}
