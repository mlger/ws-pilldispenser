# firmware-coproc

Dispense-mechanism co-processor firmware. **MCU decision deferred to Phase 3**
(spec C2):

| Option | Pros | Cons |
| --- | --- | --- |
| ESP32-C3 | same toolchain as firmware-main (espressif/idf CI image), shared core_protocol trivially | RISC-V single core, no PIO |
| RP2040 | cheap, dual M0+, PIO for stepper pulses, UF2 drag-drop flashing | second toolchain (pico-sdk) in CI |
| STM32G0 | industrial availability, proven motor control ecosystem | STM32CubeProgrammer for flashing, no web flasher |

## What exists today

Nothing buildable. The shared `core_protocol` codec (PD-Link v1,
`firmware-main/components/core_protocol`) is the contract both sides implement,
and its Unity tests run on the host for both projects (spec C2: "protocol codec
tests identical on both sides").

## Phase 3 checklist

- [ ] Choose the MCU (update `docs/architecture.md` "Decisions deferred")
- [ ] Wire `ci-coproc.yml` (currently gated by `hashFiles('firmware-coproc/**/CMakeLists.txt')`)
- [ ] Vendor the shared codec sources + tests from `firmware-main/components/core_protocol`
- [ ] HIL jig tests (BER, CRC, timeouts) in `hil.yml`
