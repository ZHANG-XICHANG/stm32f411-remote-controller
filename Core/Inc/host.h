#ifndef HOST_H
#define HOST_H

#include "telemetry.h"

/* Task context only. Nonblocking: 1 = queued, 0 = invalid/unavailable/full.
 * Queued does not mean delivered to the PC. Uses the existing USB output queue,
 * independently of DEBUG_PRINT_ENABLE. CSV values are raw integers.
 */
/* throttle: current remote command, 0..1000, not motor PWM or flight feedback. */
uint8_t Host_SendTelemetry(const TelemetryData_t *data);
uint8_t Host_SendTelemetrySequence(uint32_t sequence);

#endif
