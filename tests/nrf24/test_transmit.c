#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../../Core/Inc/ack_payload.h"
#include "nRF24L01P.h"
#include "spi.h"
#include "cmsis_os2.h"

SPI_HandleTypeDef hspi2;
static uint8_t regs[32], command, payload[32], outcome;
static unsigned int byteIndex, payloadSize, spiCalls, powerDowns, osWaits, halWaits;
static uint32_t now, triggeredAt;
static int ce, active, running;
static int csn;
static unsigned int failAt, byteDelay;
static HAL_StatusTypeDef injectedError;
static uint8_t rxPackets[3][32], rxWidths[3];
static unsigned int rxHead, rxCount, rxFlushes;
static uint32_t printedSequences[4];
static unsigned int printedCount, invalidCount;

static void advance(uint32_t ms)
{
    now += ms;
    if (active && outcome && (uint32_t)(now - triggeredAt) >= 4U)
    {
        regs[L01REG_STATUS] |= outcome;
        if (outcome == (1U << TX_DS)) payloadSize = 0;
        active = 0;
    }
}

void HAL_GPIO_WritePin(int port, int pin, int value)
{
    (void)port;
    if (pin == nRF24_CSN_Pin)
    {
        if (value == GPIO_PIN_SET && !csn && command == R_RX_PAYLOAD &&
            rxCount && byteIndex == (unsigned int)rxWidths[rxHead] + 1U)
        {
            rxHead = (rxHead + 1U) % 3U;
            --rxCount;
        }
        csn = value;
        if (value == GPIO_PIN_RESET) byteIndex = 0;
    }
    if (pin != nRF24_CE_Pin) return;
    if (!value && ce && !(regs[L01REG_CONFIG] & (1U << PRIM_RX)))
        assert((uint32_t)(now - triggeredAt) >= 1U);
    ce = value;
    if (value && !(regs[L01REG_CONFIG] & (1U << PRIM_RX)))
    {
        assert(regs[L01REG_CONFIG] & (1U << PWR_UP));
        assert(payloadSize == FIXED_PACKET_LEN);
        triggeredAt = now;
        active = 1;
    }
}

int HAL_GPIO_ReadPin(int port, int pin) { (void)port; (void)pin; return 1; }
uint32_t HAL_GetTick(void) { return now; }
void HAL_Delay(uint32_t ms) { halWaits++; advance(ms + 1U); }
void Error_Handler(void) { assert(!"Unexpected SPI error"); }
void Debug_Print(const char *text) { (void)text; }
void Debug_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (strcmp(format, "ACK_SEQ,%lu\r\n") == 0)
    {
        assert(printedCount < 4U);
        printedSequences[printedCount++] = (uint32_t)va_arg(args, unsigned long);
    }
    else if (strcmp(format, "ACK_INVALID_LEN,%u\r\n") == 0) ++invalidCount;
    va_end(args);
}
int osKernelGetState(void) { return running ? osKernelRunning : osKernelInactive; }
uint32_t osKernelGetTickFreq(void) { return 1000U; }
int osDelay(uint32_t ticks) { osWaits++; advance(ticks); return 0; }

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, const uint8_t *tx,
                           uint8_t *rx, uint16_t size, uint32_t timeout)
{
    (void)spi;
    assert(timeout > 0U && timeout <= L01_SPI_TIMEOUT_MS);
    assert(!csn);
    assert(size == 1U);
    assert(++spiCalls < 10000U); /* Detect unbounded polling. */
    if (spiCalls == failAt)
    {
        *rx = 0xFFU; /* Failed data must never be mistaken for TX success. */
        if (injectedError == HAL_TIMEOUT) advance(timeout);
        return injectedError;
    }
    advance(byteDelay);
    *rx = regs[L01REG_STATUS];
    if (byteIndex++ == 0U)
    {
        command = *tx;
        if (command == FLUSH_TX) payloadSize = 0;
        if (command == FLUSH_RX) { rxCount = 0; ++rxFlushes; }
    }
    else if ((command & 0xE0U) == W_REGISTER)
    {
        unsigned int reg = command & 0x1FU;
        if (reg == L01REG_STATUS) regs[reg] &= (uint8_t)~*tx;
        else
        {
            if (reg == L01REG_CONFIG && (regs[reg] & (1U << PWR_UP)) &&
                !(*tx & (1U << PWR_UP)))
            {
                powerDowns++;
                active = 0;
            }
            regs[reg] = *tx;
        }
    }
    else if ((command & 0xE0U) == R_REGISTER)
        *rx = (command == L01REG_FIFO_STATUS)
            ? (rxCount ? 0U : (1U << RX_EMPTY)) : regs[command & 0x1FU];
    else if (command == W_TX_PAYLOAD)
    {
        assert(payloadSize < sizeof(payload));
        payload[payloadSize++] = *tx;
    }
    else if (command == R_RX_PL_WID) *rx = rxCount ? rxWidths[rxHead] : 0U;
    else if (command == R_RX_PAYLOAD)
    {
        assert(rxCount && byteIndex - 2U < rxWidths[rxHead]);
        *rx = rxPackets[rxHead][byteIndex - 2U];
    }
    return HAL_OK;
}

static void reset(uint8_t result, uint32_t start, int scheduler)
{
    memset(regs, 0, sizeof(regs));
    regs[L01REG_CONFIG] = (1U << PWR_UP) | (1U << PRIM_RX);
    /* Old TX result must be cleared; pending RX notification must survive. */
    regs[L01REG_STATUS] = (1U << RX_DR) | (1U << TX_DS);
    outcome = result;
    now = start;
    running = scheduler;
    ce = active = 0;
    L01_SetCE(CE_LOW);
    csn = 1;
    failAt = byteDelay = 0;
    spiCalls = powerDowns = osWaits = halWaits = payloadSize = 0;
    rxHead = rxFlushes = 0;
    rxCount = 1;
    rxWidths[0] = FIXED_PACKET_LEN;
    memset(rxPackets, 0x5A, sizeof(rxPackets));
}

static void checkTransfer(uint8_t outcomeFlags, uint8_t expected, uint32_t start, int scheduler)
{
    uint8_t data[FIXED_PACKET_LEN];
    for (unsigned int i = 0; i < sizeof(data); i++) data[i] = (uint8_t)i;
    reset(outcomeFlags, start, scheduler);
    assert(L01_TransmitPacket(data, sizeof(data), 10U) == expected);
    assert(memcmp(payload, data, sizeof(data)) == 0);
    assert(payloadSize == 0U);
    assert(ce && !active);
    assert((regs[L01REG_CONFIG] & 3U) == 3U); /* Powered RX. */
    assert(regs[L01REG_STATUS] == (1U << RX_DR));
    assert(powerDowns == (expected == L01_TX_TIMEOUT ? 1U : 0U));
    assert(scheduler ? (osWaits && !halWaits) : (halWaits && !osWaits));
}

static void checkSpiErrors(void)
{
    uint8_t data[32] = {0};
    const uint8_t outcomes[] = {1U << TX_DS, 1U << MAX_RT, 0U};
    const HAL_StatusTypeDef errors[] = {HAL_ERROR, HAL_BUSY, HAL_TIMEOUT};
    for (unsigned int o = 0; o < sizeof(outcomes); ++o)
    {
        reset(outcomes[o], 0, 1);
        (void)L01_TransmitPacket(data, FIXED_PACKET_LEN, 10U);
        unsigned int calls = spiCalls;
        for (unsigned int e = 0; e < sizeof(errors) / sizeof(errors[0]); ++e)
        {
            for (unsigned int i = 1; i <= calls; ++i)
            {
                reset(outcomes[o], 0, 1);
                failAt = i;
                injectedError = errors[e];
                assert(L01_TransmitPacket(data, FIXED_PACKET_LEN, 10U) == L01_TX_SPI_ERROR);
                assert(spiCalls == i); /* Includes cleanup failures after TX success. */
                assert(csn && !ce && L01_GetCEStatus() == CE_LOW);
            }
        }
    }

    reset(0, 0, 1);
    assert(L01_Init() == HAL_OK);
    unsigned int initCalls = spiCalls;
    for (unsigned int i = 1; i <= initCalls; ++i)
    {
        reset(0, 0, 1);
        failAt = i;
        injectedError = HAL_BUSY;
        assert(L01_Init() == HAL_BUSY);
        assert(spiCalls == i && csn && !ce);
    }
    for (unsigned int i = 1; i <= 12U; ++i)
    {
        reset(0, 0, 1);
        failAt = i;
        injectedError = HAL_TIMEOUT;
        assert(L01_Check() == HAL_TIMEOUT);
        assert(spiCalls == i && csn && !ce);
    }

    for (unsigned int i = 1; i <= FIXED_PACKET_LEN + 3U; ++i)
    {
        uint8_t length = 99U;
        reset(0, 0, 1);
        memset(data, 0xA5, sizeof(data));
        failAt = i;
        injectedError = HAL_ERROR;
        assert(L01_ReadRXPayload(data, &length) == HAL_ERROR);
        assert(length == 0U && spiCalls == i && csn && !ce);
        for (unsigned int j = 0; j < sizeof(data); ++j) assert(data[j] == 0xA5U);
    }
    reset(0, 0, 1);
    failAt = 2U;
    injectedError = HAL_BUSY;
    uint8_t value = 0xA5U;
    assert(L01_ReadSingleReg(L01REG_CONFIG, &value) == HAL_BUSY);
    assert(value == 0xA5U && csn);

    /* Deadline is shared across bytes, including tick wrap. */
    reset(0, UINT32_MAX, 1);
    byteDelay = 1U;
    assert(L01_ReadMultiReg(L01REG_CONFIG, data, 5U) == HAL_TIMEOUT);
    assert(spiCalls == L01_SPI_TIMEOUT_MS && csn && !ce);

    /* A transient failure can recover through reinitialization. */
    reset(1U << TX_DS, 0, 1);
    failAt = 1U;
    injectedError = HAL_ERROR;
    assert(L01_Init() == HAL_ERROR);
    failAt = 0U;
    assert(L01_Init() == HAL_OK);
    assert(L01_Check() == HAL_OK);
    assert(L01_TransmitPacket(data, FIXED_PACKET_LEN, 10U) == L01_TX_SUCCESS);
}

static void checkRxQueueAndRates(void)
{
    uint8_t data[32], length;
    reset(0, 0, 1);
    rxCount = 3;
    for (unsigned int i = 0; i < 3U; ++i)
    {
        rxWidths[i] = FIXED_PACKET_LEN;
        memset(rxPackets[i], (int)(0x30U + i), FIXED_PACKET_LEN);
    }
    for (unsigned int i = 0; i < 3U; ++i)
    {
        assert(L01_ReadRXPayload(data, &length) == HAL_OK);
        assert(length == FIXED_PACKET_LEN);
        for (unsigned int j = 0; j < length; ++j) assert(data[j] == 0x30U + i);
        assert(rxCount == 2U - i && rxFlushes == 0U);
    }
    reset(0, 0, 1);
    rxWidths[0] = 33U;
    memset(data, 0xA5, sizeof(data));
    assert(L01_ReadRXPayload(data, &length) == HAL_ERROR);
    assert(length == 0U && rxCount == 0U && rxFlushes == 1U);
    assert(data[0] == 0xA5U);

    const L01_DRATE rates[] = {DRATE_250K, DRATE_1M, DRATE_2M};
    const uint8_t rateBits[] = {0x20U, 0x00U, 0x08U};
    for (unsigned int r = 0; r < 3U; ++r)
    {
        for (unsigned int original = 0; original <= 255U; ++original)
        {
            reset(0, 0, 1);
            regs[L01REG_RF_SETUP] = (uint8_t)original;
            assert(L01_SetDataRate(rates[r]) == HAL_OK);
            assert(regs[L01REG_RF_SETUP] == ((original & 0xD7U) | rateBits[r]));
        }
    }
    puts("PASS: queued RX packets preserved, invalid width flushed, all rates preserve unrelated bits");
}

static void checkAckPrinting(void)
{
    reset(0, 0, 1);
    printedCount = invalidCount = 0U;
    rxCount = 0U;
    assert(AckPayload_Poll() == HAL_OK && printedCount == 0U);
    rxCount = 3U;
    rxWidths[0] = rxWidths[2] = 4U;
    rxWidths[1] = 17U; /* Reject a control packet, not a sequence. */
    const uint8_t first[4] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t last[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(rxPackets[0], first, 4U);
    memcpy(rxPackets[2], last, 4U);
    regs[L01REG_STATUS] = (1U << RX_DR) | (1U << TX_DS);
    assert(AckPayload_Poll() == HAL_OK);
    assert(printedCount == 2U && invalidCount == 1U && rxCount == 0U);
    assert(printedSequences[0] == 0x12345678U && printedSequences[1] == UINT32_MAX);
    assert(regs[L01REG_STATUS] == (1U << TX_DS));
    assert(rxFlushes == 0U);
    for (unsigned int i = 1U; i <= 11U; ++i)
    {
        reset(0, 0, 1);
        printedCount = 0U;
        rxWidths[0] = 4U;
        failAt = i; injectedError = HAL_ERROR;
        assert(AckPayload_Poll() == HAL_ERROR && printedCount == 0U && !ce);
    }
    reset(0, 0, 1);
    assert(L01_Init() == HAL_OK);
    regs[L01REG_FEATURE] = 0U;
    assert(L01_Check() == HAL_ERROR);
    puts("PASS: ACK FIFO drain, empty ACK, big-endian USB log, invalid length, partial SPI read rejection, feature check");
}

int main(void)
{
    uint8_t data[32] = {0};
    reset(0, 0, 1);
    assert(L01_TransmitPacket(NULL, FIXED_PACKET_LEN, 100) == L01_TX_INVALID_PARAM);
    assert(L01_TransmitPacket(data, 0, 100) == L01_TX_INVALID_PARAM);
    assert(L01_TransmitPacket(data, 33, 100) == L01_TX_INVALID_PARAM);
#if DYNAMIC_PACKET == 0
    assert(L01_TransmitPacket(data, FIXED_PACKET_LEN - 1, 100) == L01_TX_INVALID_PARAM);
#endif
    assert(L01_TransmitPacket(data, FIXED_PACKET_LEN, 0) == L01_TX_INVALID_PARAM);
    assert(spiCalls == 0U);
    checkTransfer(1U << TX_DS, L01_TX_SUCCESS, 0, 1);
    checkTransfer(1U << MAX_RT, L01_TX_MAX_RETRY, 0, 1);
    checkTransfer((1U << TX_DS) | (1U << MAX_RT), L01_TX_MAX_RETRY, 0, 1);
    checkTransfer(0, L01_TX_TIMEOUT, 0, 1);
    checkTransfer(0, L01_TX_TIMEOUT, UINT32_MAX - 4U, 1);
    checkTransfer(1U << TX_DS, L01_TX_SUCCESS, UINT32_MAX - 4U, 0);
    reset(0, 0, 1);
    regs[L01REG_CONFIG] &= (uint8_t)(0xFFU ^ (1U << PWR_UP));
    assert(L01_SetPowerUp() == HAL_OK);
    assert(now >= 5U);
    checkSpiErrors();
    checkRxQueueAndRates();
    checkAckPrinting();
    puts("PASS: TX success/retry/timeout, SPI fault injection, init/recovery, atomic RX, shared deadline, tick wrap, CE timing, invalid arguments, RTOS/HAL waits");
    return 0;
}
