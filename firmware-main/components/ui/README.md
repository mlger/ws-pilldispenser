# ui (Phase 1+)

LVGL-based screens on the 800x480 RGB display. Phase 0.5 spike (b) proves
`esp_lcd_qemu_rgb` under QEMU; screen work starts in Phase 1 with the
`hal_display` / `hal_touch` contracts. UI golden-image tests dump the
framebuffer via `sim fb dump` (docs/test-plan.md ui suite).
