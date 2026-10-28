# pcf85063 driver (Phase 1)

PCF85063 RTC driver (I2C). Wrapped by `hal_rtc`. HIL test: drift over 24 h;
boot check asserts the oscillator-stopped flag is clear (FMEA-06).
