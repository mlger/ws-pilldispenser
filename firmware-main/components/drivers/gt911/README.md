# gt911 driver (Phase 1)

GT911 capacitive touch controller driver (I2C GPIO8/9, IRQ GPIO4, RST via
CH422G EXIO1). Wrapped by `hal_touch`; the hal_sim backend injects scripted
events from the `sim touch` console command / scenario files.
