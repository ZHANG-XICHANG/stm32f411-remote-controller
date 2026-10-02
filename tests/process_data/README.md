# Power event tests

Compile the actual `process_data.c` and `transmit.c` using host stubs for RTOS
critical sections and RF transmission. Tests cover the 0/4/5/1000 throttle
boundaries, release vs. press/hold, multiple button bits, startup, preservation
until transmission, and exactly one packet attempt carrying the event.
Pitch/roll trim tests also cover release-only updates, cumulative signed offsets,
independent axes, preservation across joystick updates, simultaneous buttons,
packet encoding, output clamping and offset saturation.

From the repository root in a Visual Studio x64 Native Tools Command Prompt:

```bat
if not exist build\process-data-tests mkdir build\process-data-tests
cl /nologo /utf-8 /std:c11 /W4 /D__MAIN_H /FIstdint.h /Itests/process_data/stubs /ICore/Inc Core/Src/process_data.c Core/Src/transmit.c tests/process_data/test_power_event.c /Febuild/process-data-tests/test_power_event.exe /Fobuild/process-data-tests/
build\process-data-tests\test_power_event.exe
```

Keep assertions enabled. These tests do not exercise GPIO debounce, actual
task scheduling or RF delivery. Test stubs must not enter firmware include paths.
