# LCD1602 driver tests

Compile the actual driver with simulated HAL I2C and CMSIS-RTOS calls. Tests check
the initialization sequence, E edges with stable data/control bits, all 255 non-NUL
byte values, empty/null strings, cursor bounds, backlight preservation, display
controls, failure at each initialization transfer, mid-character failure, recovery
by re-initialization, and both pre-scheduler and RTOS waits (100/1000 Hz ticks).

From the repository root in a Visual Studio x64 Native Tools Command Prompt:

```bat
if not exist build\lcd1602-tests mkdir build\lcd1602-tests
cl /nologo /utf-8 /std:c11 /W4 /FIhal_mock.h /Itests/lcd1602/stubs /ICore/Inc Core/Src/lcd1602_i2c.c tests/lcd1602/test_lcd1602.c /Febuild/lcd1602-tests/test_lcd1602.exe /Fobuild/lcd1602-tests/
build\lcd1602-tests\test_lcd1602.exe
```

Keep assertions enabled. Tests validate emitted I2C bytes and requested delays;
they do not validate physical voltage levels, LCD glass, expander propagation or
real task scheduling. Stub headers must not enter firmware include paths.
