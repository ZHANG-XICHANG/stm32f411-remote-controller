#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "process_data.h"
#include "joystick.h"
#include "transmit.h"
#include "nRF24L01P.h"

JoystickData_t joystickData;
static unsigned int criticalDepth;
static uint8_t packet[FIXED_PACKET_LEN];
static uint8_t txResult = L01_TX_SUCCESS;

void TestEnterCritical(void) { criticalDepth++; }
void TestExitCritical(void) { assert(criticalDepth > 0); criticalDepth--; }

/* Debug transport is outside this test; logging must not hold the data lock. */
void Debug_PrintfEveryN(uint32_t *counter, uint32_t every_n, const char *format, ...)
{
    (void)counter;
    (void)every_n;
    (void)format;
    assert(criticalDepth == 0);
}

uint8_t L01_TransmitPacket(uint8_t *data, uint8_t size, uint32_t timeout_ms)
{
    assert(criticalDepth == 0);
    assert(size == sizeof(packet));
    assert(timeout_ms > 0);
    memcpy(packet, data, size);
    return txResult;
}

static void throttle(uint16_t value)
{
    joystickData.thr = value;
    joystickData.yaw = 501;
    joystickData.pitch = 502;
    joystickData.roll = 503;
    ProcessJoystickData();
    assert(remoteData.thr == value);
    assert(remoteData.yaw == 501 && remoteData.pitch == 502 && remoteData.roll == 503);
}

static void buttons(uint8_t pressed, uint8_t held, uint8_t released)
{
    ButtonEvent_t event = {pressed, held, released};
    ProcessButtonData(event);
    assert(criticalDepth == 0);
}

int main(void)
{
    /* Startup zero is not a measured throttle value. */
    buttons(0, 0, Button_6);
    assert(remoteData.power == 0);

    throttle(4);
    buttons(Button_6, 0, 0);
    buttons(0, Button_6, 0);
    buttons(0, 0, Button_5);
    assert(remoteData.power == 0);

    /* A matching release latches until a packet takes the event. */
    remoteData.fixheight = 1;
    buttons(0, 0, Button_6 | Button_5);
    assert(remoteData.power == 1 && remoteData.fixheight == 1);
    buttons(0, 0, 0);
    throttle(4);
    assert(remoteData.power == 1);
    assert(TransmitData() == L01_TX_SUCCESS);
    assert(packet[12] == 1 && remoteData.power == 0);
    assert(packet[11] == 1 && remoteData.fixheight == 0);
    assert(packet[3] == 0 && packet[4] == 4);
    buttons(0, 0, 0);
    (void)TransmitData();
    assert(packet[12] == 0);

    /* Strict boundary: 5 and higher do not trigger; 0 does. */
    throttle(5);
    buttons(0, 0, Button_6);
    assert(remoteData.power == 0);
    throttle(1000);
    buttons(0, 0, Button_6);
    assert(remoteData.power == 0);
    throttle(0);
    buttons(0, 0, 0);
    assert(remoteData.power == 0); /* Lowering throttle is not a release. */
    buttons(0, 0, Button_6);
    assert(remoteData.power == 1);

    /* Existing semantics: one packet attempt, even if RF delivery fails. */
    txResult = L01_TX_MAX_RETRY;
    assert(TransmitData() == L01_TX_MAX_RETRY);
    assert(packet[12] == 1 && remoteData.power == 0);
    (void)TransmitData();
    assert(packet[12] == 0);
    assert(criticalDepth == 0);

    /* Pitch/roll trim reacts to release only and accumulates per axis. */
    joystickData.pitch = 500;
    joystickData.roll = 500;
    ProcessJoystickData();
    buttons(Button_1 | Button_3, 0, 0);
    buttons(0, Button_1 | Button_3, 0);
    assert(remoteData.pitch == 500 && remoteData.roll == 500);
    buttons(0, 0, Button_1);
    assert(remoteData.pitch == 510 && remoteData.roll == 500);
    buttons(0, 0, Button_1);
    buttons(0, 0, Button_1);
    assert(remoteData.pitch == 530);
    buttons(0, 0, Button_2);
    buttons(0, 0, Button_3);
    assert(remoteData.pitch == 520 && remoteData.roll == 510);
    buttons(0, 0, Button_4);
    assert(remoteData.roll == 500);

    /* Sampling again retains the offset without adding it repeatedly. */
    for (unsigned int i = 0; i < 20; i++) ProcessJoystickData();
    assert(remoteData.pitch == 520 && remoteData.roll == 500);
    assert(joystickData.pitch == 500 && joystickData.roll == 500);
    joystickData.pitch = 600;
    ProcessJoystickData();
    assert(remoteData.pitch == 620);
    buttons(0, 0, Button_1 | Button_2 | Button_3 | Button_4);
    assert(remoteData.pitch == 620 && remoteData.roll == 500);

    /* Concurrent release bits can adjust both axes and latch power. */
    buttons(0, 0, Button_1 | Button_3 | Button_6);
    assert(remoteData.pitch == 630 && remoteData.roll == 510 && remoteData.power == 1);
    txResult = L01_TX_SUCCESS;
    assert(TransmitData() == L01_TX_SUCCESS);
    assert(((packet[7] << 8) | packet[8]) == 630);
    assert(((packet[9] << 8) | packet[10]) == 510);
    assert(packet[12] == 1 && remoteData.power == 0);

    /* Output and stored offset saturate without unsigned wraparound. */
    for (unsigned int i = 0; i < 300; i++) buttons(0, 0, Button_1 | Button_3);
    assert(remoteData.pitch == 1000 && remoteData.roll == 1000);
    buttons(0, 0, Button_1 | Button_2 | Button_3 | Button_4);
    for (unsigned int i = 0; i < 100; i++) buttons(0, 0, Button_2 | Button_4);
    assert(remoteData.pitch == 600 && remoteData.roll == 500);
    for (unsigned int i = 0; i < 300; i++) buttons(0, 0, Button_2 | Button_4);
    assert(remoteData.pitch == 0 && remoteData.roll == 0);
    buttons(0, 0, Button_1 | Button_2 | Button_3 | Button_4);
    for (unsigned int i = 0; i < 100; i++) buttons(0, 0, Button_1 | Button_3);
    assert(remoteData.pitch == 600 && remoteData.roll == 500);
    assert(remoteData.thr == 0 && remoteData.yaw == 501 && remoteData.power == 0);
    puts("PASS: power event boundaries and consumption; trim accumulation, release-only, axis mapping, persistence, packet encoding, saturation");
    return 0;
}
