#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "telemetry_wire.h"

void Telemetry_Init(void);
void Telemetry_Update(const TelemetryData_t *data);
const TelemetryData_t *Telemetry_Get(void);

/* Call from the communication task. Returns 1 if queued for USB, else 0.
 * Publish full telemetry only after all fields were decoded successfully.
 */
uint8_t Telemetry_Publish(const TelemetryData_t *data);
uint8_t Telemetry_PublishSequence(uint32_t sequence);

#endif
