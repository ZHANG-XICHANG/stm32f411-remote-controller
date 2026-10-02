#include "connection.h"

#define CONNECTION_TIMEOUT_MS  300U

static uint32_t lastSuccessTick = 0;
static uint8_t everConnected = 0;

void Connection_Init(void)
{
    lastSuccessTick = HAL_GetTick();
    everConnected = 0;
}

void Connection_ReportSuccess(void)
{
    lastSuccessTick = HAL_GetTick();
    everConnected = 1;
}

ConnectionState_t Connection_GetState(void)
{
    if (!everConnected)
    {
        return CONNECTION_DISCONNECTED;
    }

    if ((HAL_GetTick() - lastSuccessTick) >= CONNECTION_TIMEOUT_MS)
    {
        return CONNECTION_DISCONNECTED;
    }

    return CONNECTION_CONNECTED;
}