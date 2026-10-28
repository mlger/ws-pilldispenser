# QEMU Testing Guide (ESP32-S3)

Status: Phase 0.5. Baseline: ESP-IDF v5.5.6 (verified). Follows the official Espressif QEMU guide.

Espressif's QEMU fork emulates the ESP32-S3, so most firmware logic runs in CI without hardware. The QEMU overlay `sdkconfig.qemu` selects `PD_HAL_SIM=y`, OpenCores Ethernet and UART0 console; nothing under QEMU touches real eFuses.

## Prerequisites and installation

Verified against the ESP-IDF QEMU guide (stable):

```bash
sudo apt-get install -y libgcrypt20 libglib2.0-0 libpixman-1-0 \
     libsdl2-2.0-0 libslirp0
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa qemu-riscv32
. $IDF_PATH/export.sh
```

`idf_tools.py` installs the QEMU binaries that match the pinned ESP-IDF release, giving you `qemu-system-xtensa` (the `esp32s3` machine).

## Building for QEMU

```bash
cd firmware-main
idf.py -B build_qemu -DSDKCONFIG=build_qemu/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.qemu" \
  build
```

Shortcut: `tools/qemu/run_qemu.sh` wraps `idf.py qemu` with the project defaults above (build dir `build_qemu`, the same `SDKCONFIG_DEFAULTS` chain).

| Overlay | Role |
| --- | --- |
| `sdkconfig.defaults` | common settings |
| `sdkconfig.defaults.esp32s3` | target board: 16 MB flash, 8 MB octal PSRAM, console UART0 |
| `sdkconfig.qemu` | `PD_HAL_SIM=y`, open_eth, console UART0 (never USB-Serial-JTAG), no esp_wifi/BT, no ADC calibration at boot, `SPIRAM_IGNORE_NOTFOUND`, DIO flash mode, 16 MB flash |

## Interactive usage

`idf.py qemu monitor` builds if needed, starts QEMU and attaches the IDF monitor. All commands take `-B build_qemu`:

| Command | Purpose |
| --- | --- |
| `idf.py -B build_qemu qemu --qemu-extra-args="-nic user,model=open_eth" monitor` | console with user-mode networking (host reachable at 10.0.2.2) |
| `idf.py -B build_qemu qemu --graphics monitor` | opens a virtual framebuffer window; an esp_lcd-compatible driver component is available for the virtual framebuffer (sim_display targets 800x480 RGB565) |
| `idf.py -B build_qemu qemu gdb` | GDB server debugging |
| `idf.py -B build_qemu qemu efuse-summary` | read the emulated eFuses |
| `idf.py -B build_qemu qemu --flash-file my_16mb.bin monitor` | boot a custom flash image; the image size must match `CONFIG_ESPTOOLPY_FLASHSIZE` exactly (16 MB here) |

eFuse emulation is supported, so flash-encryption and secure-boot code paths can be exercised without irreversible operations on real silicon.

### PSRAM leg (gated by spike)

Used only after the Phase 0.5 spike passes; pairs with the `sdkconfig.qemu.psram` overlay:

```bash
idf.py -B build_qemu qemu monitor \
  --qemu-extra-args="-m 8M -global driver=ssi_psram,property=is_octal,value=true -nic user,model=open_eth"
```

VERIFY: Phase 0.5 spike (a) must confirm octal PSRAM emulation on the esp32s3 machine; see docs/spikes/phase0.5-spikes.md

## Automated tests (pytest-embedded)

Install the pinned Python dependencies first: `pip install -r firmware-main/test/requirements.txt` (pytest-embedded `==2.9.3` family, Python >= 3.10).

pytest-embedded 2.x shells out to `esptool.py merge-bin` (esptool v5 CLI) to
build the flash image; the IDF environment ships esptool 4.x, so upgrade the
pip esptool first: `pip install -U esptool`. The test apps must also be built
before running the suite (pytest-embedded does not build them).

```bash
# 1. Build the test apps (per-leg sdkconfig, mirrors CI)
(cd firmware-main/test/apps/unit     && idf.py -B build_qemu -DSDKCONFIG=build_qemu/sdkconfig \
   -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.qemu" build)
(cd firmware-main/test/apps/scenario && idf.py -B build_qemu -DSDKCONFIG=build_qemu/sdkconfig \
   -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.qemu" build)

# 2. Run the suites (--build-dir is resolved relative to --app-path)
pytest firmware-main/test -m qemu \
  --embedded-services idf,qemu --target esp32s3 \
  --app-path firmware-main/test/apps/unit --build-dir build_qemu \
  --junitxml=reports/qemu-unit.xml
pytest firmware-main/test -m qemu \
  --embedded-services idf,qemu --target esp32s3 \
  --app-path firmware-main/test/apps/scenario --build-dir build_qemu \
  --qemu-extra-args="-nic user,model=open_eth" \
  --junitxml=reports/qemu-scenario.xml
```

NOTE: do not keep a persistent `firmware-main/sdkconfig` between leg builds —
an existing project sdkconfig overrides `SDKCONFIG_DEFAULTS` and silently
rebuilds a leg with the previous configuration. CI never hits this (fresh
checkout); pass `-DSDKCONFIG=build_<leg>/sdkconfig` locally (as above).

Example from `firmware-main/test/test_scenarios_qemu.py`:

```python
import pytest
from pytest_embedded_idf.dut import IdfDut

@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_unity_components(dut: IdfDut) -> None:
    dut.run_all_single_board_cases(timeout=120)

@pytest.mark.qemu
def test_dose_due_across_eu_dst(dut: IdfDut) -> None:
    dut.expect_exact('PD_READY', timeout=60)
    dut.write('sim tz CET-1CEST,M3.5.0,M10.5.0/3')
    dut.write('sim time set 2026-03-29T00:59:50Z')   # DST jump
    dut.write('sim coproc auto-ack on')
    dut.expect(r'FSM: DOSE_DUE slot=\d+', timeout=30)
    dut.expect_exact('FSM: DISPENSED', timeout=30)
    dut.expect_exact('MQTT: event published dose', timeout=30)
```

`IdfDut` is imported from `pytest_embedded_idf`. `dut.run_all_single_board_cases(timeout=...)` parses the numbered unity test menu printed by the app and runs every case.

### Required suites

| Suite | Scope | Introduced |
| --- | --- | --- |
| unit | every component's Unity TEST_CASEs (scheduler, fsm, storage, comms, ota, coproc_link codec) driven through the test menu (`test_unit_qemu.py`) | Phase 0.5 skeleton; required from Phase 2 |
| scenarios | dose due/missed/snoozed, double-dose guard, jam/empty from sim_coproc, power-loss mid-dispense (kill QEMU, restart with the same `qemu_flash.bin`, verify NVS journal replay), offline queue flush when the broker returns, DST spring/fall for EU and US timezones, leap day (`test_scenarios_qemu.py`) | Phase 2 (coproc faults complete in Phase 3) |
| mqtt | mosquitto on the runner with the test CA; firmware connects to `10.0.2.2:8883`; verifies TLS, LWT, retained state, reconnect backoff | Phase 4 |
| ui | boots headless in CI; `sim fb dump` writes a PNG; tolerance-based golden image diff for each main screen | harness in Phase 0.5 (spike b); grows as screens land |
| ota | host HTTP server serves the v+1 image; device updates, reboots, self-test fails on purpose, bootloader rolls back and the test expects the previous version marker; second case: self-test passes and `esp_ota_mark_app_valid_cancel_rollback` is called (`pytest_ota_rollback_qemu.py`) | mechanism in Phase 0.5 (spike d); suite required in Phase 6 |
| security | emulated eFuses only: flash encryption dev-mode boot with a test key via `--qemu-efuse-path` / encrypted image; secure boot v2 case marked xfail until proven on S3 (`pytest_security_qemu.py`) | Phase 0.5 (spike e); secure boot unblocked at Phase 7 |

## Scenario authoring

`CONFIG_PD_SIM_CONSOLE` (depends on `PD_HAL_SIM=y`) enables the `sim ...` console commands; `CONFIG_PD_TEST_HOOKS` exposes the deterministic markers (`PD_READY`, `FSM: <state>`) that tests expect.

| Command | Effect |
| --- | --- |
| `sim touch x y [down\|up]` | inject a touch event into sim_touch |
| `sim time set <ISO8601>` | move the sim_rtc clock for DST and time-edge tests |
| `sim tz <POSIX TZ>` | set the timezone, e.g. `CET-1CEST,M3.5.0,M10.5.0/3` |
| `sim coproc auto-ack on\|off` | sim_coproc answers dispense frames automatically |
| `sim coproc fault <type>` | inject ack / jam / empty / timeout / CRC faults |
| `sim fb dump` | dump the framebuffer to PNG for golden-image comparison |

Scenario files under `firmware-main/test/fixtures/scenarios` script the same operations (touch sequences, time and timezone changes, coproc faults) so a whole user flow replays deterministically. The firmware can also load a scenario from LittleFS via `CONFIG_PD_SIM_SCENARIO_PATH`.

## CI integration

- QEMU consumes about 1 host core per guest CPU: run suites serially within a job and shard across matrix jobs.
- Every test carries a timeout; a hung QEMU boot must fail fast instead of eating the job timeout.
- On failure the job uploads JUnit XML, QEMU stdout and `qemu_flash.bin`.
- The `qemu-tests` job consumes the `build[qemu]` leg artifact from the build matrix.
- The nightly workflow runs the ESP-IDF canary leg (v6.1-rc1 docker image, non-blocking) plus a long QEMU soak.

## Known QEMU limitations

| Limitation | Consequence | Handling |
| --- | --- | --- |
| No Wi-Fi/BT emulation | SIM build never links esp_wifi/BT; hal_net is stubbed | provisioning tests are HIL-only |
| TF card not emulated | SD_CS is driven by CH422G EXIO4 over I2C | sim_storage_ext RAM/flash stub; card tests HIL-only |
| ADC calibration avoided at boot | QEMU overlay skips boot-time ADC calibration | battery/sensor ADC tests HIL-only |
| USB-Serial-JTAG console is a stub in QEMU | firmware printing to USB-Serial-JTAG boots silent | `sdkconfig.qemu` always forces console UART0 |
| Secure boot v2 on S3 | docs conflict on support maturity | treated as experimental; xfail test case; manual ceremony only |
| PSRAM emulation unproven | octal PSRAM may not come up on the esp32s3 machine | default QEMU legs run without PSRAM (`SPIRAM_IGNORE_NOTFOUND`); PSRAM leg behind spike (a) |
| Flash mode QIO unverified | timing behavior may differ under QEMU | `sdkconfig.qemu` pins DIO as a precaution |

## Appendix: open VERIFY items

| Item | Spike | Tracked in |
| --- | --- | --- |
| open_eth networking on the esp32s3 machine, host broker at 10.0.2.2 | spike (c) | docs/spikes/phase0.5-spikes.md |
| octal PSRAM emulation (`-m 8M` + `ssi_psram is_octal=true`) | spike (a) | docs/spikes/phase0.5-spikes.md |
| esp_lcd_qemu_rgb at 800x480 RGB565 with LVGL | spike (b) | docs/spikes/phase0.5-spikes.md |

All three must be resolved in Phase 0.5 before the dependent suites become required checks.
