#ifndef PID_COMMAND_H
#define PID_COMMAND_H
#include "pid_wire.h"
/* USB callback copies bytes only; parsing/radio/printing run in task context. */
void PidCommand_USB(const uint8_t *p,uint32_t n);
void PidCommand_Poll(void);
uint8_t PidCommand_Send(uint8_t *tx_status);
void PidCommand_Ack(const uint8_t *p,uint8_t length);
#endif
