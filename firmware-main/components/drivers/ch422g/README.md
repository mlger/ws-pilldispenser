# ch422g driver (Phase 1)

I2C IO expander driver for the CH422G (0x24): EXIO2 = DISP (backlight),
EXIO4 = SD_CS, EXIO1 = TP_RST, DI0/DI1 inputs. Wrapped by `hal_ioexp`;
the hal_sim backend keeps an in-RAM register model with write logging so
contract tests can compare both backends.
