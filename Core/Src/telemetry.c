#include "telemetry.h"
#include "host.h"
#include "transmit.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static TelemetryData_t telemetry_data;

void Telemetry_Init(void)
{
    memset(&telemetry_data, 0, sizeof(telemetry_data));
}

void Telemetry_Update(const TelemetryData_t *data)
{
    if (data == NULL)
    {
        return;
    }

    telemetry_data = *data;
}

const TelemetryData_t *Telemetry_Get(void)
{
    return &telemetry_data;
}

uint8_t Telemetry_Publish(const TelemetryData_t *data)
{
    if (data == NULL) return 0U;
    Telemetry_Update(data);
    return Host_SendTelemetry(data);
}

uint8_t Telemetry_PublishSequence(uint32_t sequence)
{
    /* A sequence-only ACK must not fabricate valid PID measurements. */
    return Host_SendTelemetrySequence(sequence);
}
