#include "transmit.h"
#include "nRF24L01P.h"
#include "FreeRTOS.h"
#include "task.h"

RemoteData_t remoteData = {0};  //遠程數據

uint8_t com_buff[FIXED_PACKET_LEN] = {0};  //通訊數據緩衝區

/**
 * @brief 發送數據
 *
 * 本函式會將處理後的搖桿數據通過 nRF24L01P 發送出去。
 * @return 0x20 成功、0x10 達最大重傳次數、0x01 逾時、0x02 參數錯誤、0x03 SPI 錯誤。
 * @note 須先成功初始化並檢查；由單一 Task 呼叫。SPI 錯誤時 CE 保持低，須重新初始化。
 */
uint8_t TransmitData(void)
{
    static uint32_t debugCounter = 0;
    //1.發送數據=>唯一性和可靠性
    //唯一性=>幀頭較驗指定發送給對應設備
    //可靠性=>數據結尾添加較驗碼較驗合(將數據累加後的值添加到數據結尾)
    RemoteData_t snapshot;
    uint32_t checksum = 0;

    /*
     * 臨界區只負責複製資料，時間非常短。
     * ProcessJoystickData() 無法在複製到一半時更新 remoteData。
     */
    taskENTER_CRITICAL();
    snapshot = remoteData;

    /*
     * fixheight 與 power 都是單次事件。
     * 它們已保存在 snapshot 中，因此清零後仍會出現在本次封包，
     * 下一個傳送週期則恢復為 0。
     */
    remoteData.fixheight = 0U;
    remoteData.power = 0U;
    taskEXIT_CRITICAL();

    /*
     * 17-byte 封包配置：
     * [0..2] 幀頭、[3..10] 四軸、[11] 定高、[12] 開關機、
     * [13..16] 32-bit checksum。
     */
    com_buff[0] = FRAME_HEADER_1;
    com_buff[1] = FRAME_HEADER_2;
    com_buff[2] = FRAME_HEADER_3;

    /* 後面全部讀取 snapshot，不再直接讀 remoteData */
    com_buff[3]  = (snapshot.thr >> 8) & 0xFFU;
    com_buff[4]  = snapshot.thr & 0xFFU;

    com_buff[5]  = (snapshot.yaw >> 8) & 0xFFU;
    com_buff[6]  = snapshot.yaw & 0xFFU;

    com_buff[7]  = (snapshot.pitch >> 8) & 0xFFU;
    com_buff[8]  = snapshot.pitch & 0xFFU;

    com_buff[9]  = (snapshot.roll >> 8) & 0xFFU;
    com_buff[10] = snapshot.roll & 0xFFU;

    com_buff[11] = snapshot.fixheight; /* 定高單次事件。 */
    com_buff[12] = snapshot.power;     /* 開關機單次事件。 */

    /* checksum 涵蓋幀頭及全部控制資料，也就是 byte 0～12。 */
    for (int i = 0; i < 13U; i++)
    {
        checksum += com_buff[i];
    }
    //高位在前
    com_buff[13] = (checksum >> 24) & 0xFF; // 高8位
    com_buff[14] = (checksum >> 16) & 0xFF;
    com_buff[15] = (checksum >> 8) & 0xFF;
    com_buff[16] = checksum & 0xFF;          // 低8位

    /* 在臨界區之外發送，等待結果並恢復接收模式。 */
    uint8_t txStatus = L01_TransmitPacket(com_buff, FIXED_PACKET_LEN, L01_TX_TIMEOUT_MS);

    /* 使用本次封包快照；remoteData 的單次事件此時已清零。 */
    Debug_PrintfEveryN(
        &debugCounter,
        20,
        "TransmitData: THR=%u YAW=%u PIT=%u ROL=%u FIX=%u PWR=%u\r\n",
        (unsigned int)snapshot.thr,
        (unsigned int)snapshot.yaw,
        (unsigned int)snapshot.pitch,
        (unsigned int)snapshot.roll,
        (unsigned int)snapshot.fixheight,
        (unsigned int)snapshot.power
    );
    return txStatus;

}
