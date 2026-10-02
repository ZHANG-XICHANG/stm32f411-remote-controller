#ifndef ACK_PAYLOAD_H
#define ACK_PAYLOAD_H

#include "nRF24L01P.h"

/* 由通訊任務在發送後呼叫；排空最多三筆 ACK，透過 USB Debug Queue 印出。
 * 空 ACK 不算錯誤；SPI 錯誤回傳給通訊任務重新初始化。
 */
HAL_StatusTypeDef AckPayload_Poll(void);

#endif
