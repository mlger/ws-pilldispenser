# storage (Phase 2)

Persistence: NVS for the dose journal (write-ahead, power-loss replay —
FMEA-04), LittleFS on the `littlefs` partition for schedule/config, TF card
via `hal_storage_ext` for exports (HIL-only).
