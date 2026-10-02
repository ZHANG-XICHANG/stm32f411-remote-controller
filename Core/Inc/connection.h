#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "main.h"

typedef enum
{
    CONNECTION_DISCONNECTED = 0,
    CONNECTION_CONNECTED
} ConnectionState_t;

void Connection_Init(void);

void Connection_ReportSuccess(void);

ConnectionState_t Connection_GetState(void);

#endif