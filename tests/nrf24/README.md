# nRF24 transmit tests

These tests compile the actual driver with simulated SPI registers, GPIO and time.
They cover successful TX, maximum retries, timeout cancellation, stale TX flags,
RX flag preservation, tick wrap, invalid arguments, CE high duration, startup
delay, and both RTOS and pre-scheduler waits. They do not validate RF hardware,
electrical timing or RTOS scheduling on the board.

Fault injection covers HAL_ERROR, HAL_BUSY and HAL_TIMEOUT at every byte of
successful, retry-exhausted and timed-out TX paths (including RX restoration).
It also checks initialization/check failures, discarded partial RX data, GPIO
cleanup, shared transaction deadlines across tick wrap, and recovery after a
transient failure. Error_Handler is an assertion trap and must never be called.

Run from the repository root in a Visual Studio x64 Native Tools Command Prompt:

```bat
if not exist build\nrf24-tests mkdir build\nrf24-tests
cl /nologo /utf-8 /std:c11 /W4 /Itests/nrf24/stubs /ICore/Inc /FInRF24L01P.h Core/Src/nRF24L01P.c Core/Src/ack_payload.c tests/nrf24/test_transmit.c /Febuild/nrf24-tests/test_transmit.exe /Fobuild/nrf24-tests/
build\nrf24-tests\test_transmit.exe
```

Keep assertions enabled (do not define `NDEBUG`). The stub headers are for this
test command only and must not be added to the firmware include paths.

ACK tests use the default dynamic-payload configuration and compile the real
ack_payload.c. They cover empty ACKs, three queued payloads after IRQ clearing,
four-byte big-endian decoding (including UINT32_MAX), USB Debug_Printf arguments,
invalid payload lengths, partial SPI failures, and ACK feature readback.
USB bus transfer and the over-the-air link still require hardware validation.
