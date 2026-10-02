#ifndef TRANSMIT_H
#define TRANSMIT_H

#include <stdint.h>
#include "debug.h"
//定義幀頭較驗值
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0x55
#define FRAME_HEADER_3 0xAA

typedef struct
{
    uint16_t thr;       /* 油門，2 bytes，映射範圍 0～1000。 */
    uint16_t yaw;       /* 偏航，2 bytes，映射範圍 0～1000。 */
    uint16_t pitch;     /* 俯仰，2 bytes，映射範圍 0～1000。 */
    uint16_t roll;      /* 滾轉，2 bytes，映射範圍 0～1000。 */
    uint8_t fixheight;  /* 定高單次事件，封包 byte 11。 */
    uint8_t power;      /* 開關機單次事件，封包 byte 12。 */
} RemoteData_t;
/* 有效控制資料共 10 bytes；封包另含 3-byte 幀頭及 4-byte checksum。 */

extern RemoteData_t remoteData;

/**
 * @brief 發送數據
 *
 * 本函式會將處理後的搖桿數據通過 nRF24L01P 發送出去。
 * @return 0x20 成功、0x10 達最大重傳次數、0x01 逾時、0x02 參數錯誤、0x03 SPI 錯誤。
 * @note 須先成功初始化並檢查；由單一 Task 呼叫。SPI 錯誤時 CE 保持低，須重新初始化。
 */
uint8_t TransmitData(void);



#endif // TRANSMIT_H
