#include "ack_payload.h"
#include "pid_command.h"
#include "debug.h"
#include "telemetry.h"

HAL_StatusTypeDef AckPayload_Poll(void)
{
    for (uint8_t i = 0U; i < 3U; ++i)
    {
        uint8_t fifo_status;
        uint8_t payload[32];
        uint8_t length;
        HAL_StatusTypeDef status = L01_ReadSingleReg(L01REG_FIFO_STATUS, &fifo_status);
        if (status != HAL_OK) return status;
        if ((fifo_status & (1U << RX_EMPTY)) != 0U) return HAL_OK;

        status = L01_ClearIRQ(1U << RX_DR);
        if (status != HAL_OK) return status;
        status = L01_ReadRXPayload(payload, &length);
        if (status != HAL_OK) return status;
        if (length == PID_WIRE_SIZE) { PidCommand_Ack(payload,length);continue; }
        if (length == TELEMETRY_WIRE_SIZE)
        {
            TelemetryData_t sample;
            if (TelemetryWire_Decode(payload, length, &sample))
            {
                (void)Telemetry_Publish(&sample);
            }
            continue;
        }
        if (length != 4U)
        {
            Debug_Printf("ACK_INVALID_LEN,%u\r\n", (unsigned int)length);
            continue;
        }

        /* 飛控序號是大端序，不使用 struct cast 或未對齊存取。 */
        uint32_t sequence = ((uint32_t)payload[0] << 24) |
                            ((uint32_t)payload[1] << 16) |
                            ((uint32_t)payload[2] << 8) | payload[3];
        /* USB congestion must not trigger radio recovery or delay control TX. */
        (void)Telemetry_PublishSequence(sequence);
    }
    return HAL_OK;
}
