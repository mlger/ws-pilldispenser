# hardware

Mechanical/electrical documentation for the PillDispenser:

- Board: Waveshare ESP32-S3-Touch-LCD-4.3B (docs/architecture.md has the
  verified pin map).
- Dispense mechanism driven by the co-processor over RS485 (Phase 3).
- Test jig for HIL (`hil.yml`): RS485 loopback, motor + sensors, touch
  actuator or touch-injection, TF card.

To be added: mechanism CAD, BOM, wiring diagrams, jig schematics.
