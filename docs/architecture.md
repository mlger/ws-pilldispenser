# PillDispenser — System Architecture

Status: Phase 0 draft — reviewed in PRs; lives with the code.

## 1. System overview

PillDispenser is an automated pill dispenser that schedules doses, physically dispenses the
correct pills from a slot mechanism at the configured time, and reports adherence to caregivers
through a cloud backend and an Android app. The main controller, an ESP32-S3, owns the dose
scheduler, dispense state machine, UI, storage, and connectivity, and delegates the mechanical
dispense to a dedicated co-processor MCU over an RS485 link. The firmware is developed
test-first: every line of business logic runs unchanged on the host (CTest), under Espressif
QEMU, and on the real Waveshare board, with zero `#ifdef`s in application code.

## 2. Hardware platform

### 2.1 Board summary

| Item | Value |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-LCD-4.3B |
| Module | ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB octal PSRAM) |
| MCU | ESP32-S3, dual core, 240 MHz |
| Radio | Wi-Fi 2.4 GHz + BLE 5 |
| Display | 4.3" IPS, 800x480, RGB parallel interface, 65K colors, driver IC ST7262 (VERIFY: confirm panel driver IC — the vendor page does not name it) |
| Touch | GT911, 5-point capacitive, I2C |
| IO expander | CH422G on I2C; optocoupler-isolated DI/DO |
| RTC | PCF85063 on I2C |
| RS485 | Half-duplex, automatic transceiver direction switching, onboard termination resistor switches |
| CAN | TWAI controller, transceiver on board |
| TF card | SPI, chip select via CH422G EXIO4 |
| USB | Native USB (GPIO19/20) plus separate UART Type-C port for flashing with auto-download circuit |
| Power | Type-C 5 V or DC 7–36 V input; 3.7 V single-cell lithium battery connector with switch |
| Buttons | BOOT (hold while powering on to enter download mode), RESET |
| SDK | ESP-IDF v5.5.6 (pinned, blocking); canary leg v6.1-rc1 (non-blocking nightly) |

### 2.2 Pin map

| Function | Signal | GPIO / resource |
| --- | --- | --- |
| Touch I2C data | TP_SDA | GPIO8 |
| Touch I2C clock | TP_SCL | GPIO9 |
| Touch interrupt | TP_IRQ | GPIO4 |
| Touch reset | TP_RST | CH422G EXIO1 |
| RS485 receive | RS485_RXD | GPIO43 |
| RS485 transmit | RS485_TXD | GPIO44 |
| CAN transmit | CANTX | GPIO15 |
| CAN receive | CANRX | GPIO16 |
| TF card MOSI | SD_MOSI | GPIO11 |
| TF card clock | SD_SCK | GPIO12 |
| TF card MISO | SD_MISO | GPIO13 |
| TF card chip select (active low) | SD_CS | CH422G EXIO4 |
| Backlight enable | DISP | CH422G EXIO2 |
| Optocoupler-isolated input 0 | DI0 | CH422G EXIO0 |
| Optocoupler-isolated input 1 | DI1 | CH422G EXIO5 |
| Native USB | USB_DN / USB_DP | GPIO19 / GPIO20 |
| RGB LCD | RGB parallel bus (65K colors) | board routing; not bit-banged by firmware |

The CAN (TWAI) transceiver is available on the board but is not used by the Phase 0–6
firmware; it is reserved for future accessories. The CH422G also provides DO outputs (OD0..)
driving the optocoupler-isolated DO lines used by the dispense-related peripherals.

## 3. Firmware layering

```text
                 +-------------------------------------------------+
                 | main (ESP-IDF app: init, tasks, console,        |
                 | sdkconfig glue; includes only hal_*.h + core_*) |
                 +-------------------------+-----------------------+
                                           |
             calls hal_*.h only            |    calls core_* APIs directly
                                           v
   +---------------------------+   +---------------------------------+
   | components/pd_hal            |   | components/core_scheduler       |
   | interface headers only:   |   | components/core_fsm             |
   | hal_display, hal_touch,   |   | components/core_protocol        |
   | hal_ioexp, hal_rtc,       |   | components/core_inventory       |
   | hal_coproc, hal_net,      |   | pure C11, no ESP-IDF headers    |
   | hal_storage_ext,          |   | built with plain CMake (host    |
   | hal_power, hal_buzzer     |   | CTest) AND as ESP-IDF components|
   +---------------------------+   +---------------------------------+
                 |
   Kconfig choice PD_HAL_BACKEND: PD_HAL_REAL (default) | PD_HAL_SIM.
   Each backend's CMakeLists guards REQUIRES with if(CONFIG_PD_HAL_*),
   so exactly one backend links into any build.
                 |
             +-----------------------------+
             v                             v
   +--------------------+   +------------------------------+
   | components/hal_real|   | components/hal_sim           |
   | drivers/{ch422g,   |   | sim_display (esp_lcd_qemu_rgb|
   | gt911, pcf85063},  |   | RGB565 800x480), sim_touch,  |
   | esp_lcd RGB, RS485 |   | sim_ioexp, sim_rtc,          |
   | UART, Wi-Fi, SD    |   | sim_coproc, sim_net (open_eth|
             |          | broker at 10.0.2.2),         |
             |          | sim_storage_ext, sim_power,  |
             |          | sim_buzzer, sim_sensors      |
             v          |
   +--------------------+   +------------------------------+
   | Waveshare ESP32-S3-|   | Espressif QEMU (esp32s3):    |
   | Touch-LCD-4.3B     |   | esp_lcd_qemu_rgb, open_eth,  |
   | (real hardware)    |   | emulated eFuses              |
   +--------------------+   +------------------------------+
```

Layering rules (Section A of the master spec):

- Application code may include only `hal_*.h` and `core_*` headers, never driver headers.
- The SIM build never links `esp_wifi`/BT (Wi-Fi and BLE are not emulated in QEMU);
  `hal_net` exposes `connect()`/`status()` so callers stay backend-agnostic.
- `PD_TEST_HOOKS` exposes deterministic log markers (for example `PD_READY`,
  `FSM: <state>`) that pytest-embedded tests assert on.
- Release builds must assert `PD_HAL_REAL=y` and `PD_SIM_CONSOLE=n`
  (`tools/ci/check_release_sdkconfig.py` plus a static assert in `main`).

## 4. Component inventory

| Component | Language / purity | Runs on | Introduced |
| --- | --- | --- | --- |
| `core_protocol` | C11, no ESP-IDF headers | host, qemu, hardware | 0.5 (codec) |
| `core_scheduler` | C11, no ESP-IDF headers | host, qemu, hardware | 2 |
| `core_fsm` | C11, no ESP-IDF headers | host, qemu, hardware | 2 |
| `core_inventory` | C11, no ESP-IDF headers | host, qemu, hardware | 2 |
| `hal` (interface headers) | C11, headers only | shared by both backends | 0.5 |
| `hal_sim` | C11, ESP-IDF + QEMU devices | qemu | 0.5 skeleton, 1 complete |
| `hal_real` | C11, ESP-IDF | hardware | 1 |
| `drivers/{ch422g,gt911,pcf85063}` | C11, ESP-IDF | hardware | 1 |
| `main` | C11, ESP-IDF | qemu, hardware | 0.5 hello-world, 2 integrated |
| `scheduler`, `fsm` (IDF task wrappers around the `core_*` logic) | C11, ESP-IDF | qemu, hardware | 2 |
| `storage` (NVS + LittleFS journal) | C11, ESP-IDF | qemu, hardware | 2 |
| `ui` (LVGL screens) | C, LVGL | qemu (`--graphics`), hardware | 2 |
| `coproc_link` (RS485 session layer over `core_protocol`) | C11 | qemu (`sim_coproc`), hardware | 3 |
| `comms` (MQTT/TLS client, offline queue) | C11, ESP-IDF | qemu (`open_eth`), hardware (Wi-Fi) | 4 |
| `ota` (download, rollback, self-test marker) | C11, ESP-IDF | qemu, hardware | 6 |

## 5. HAL contract tests

Every `hal_*.h` interface gets a contract test that is run against **both** backends:
SIM (under QEMU) and REAL (on the HIL runner, label `hil-4.3b`). The contract tests pin
behavioral semantics — not register-level detail — so `hal_real` and `hal_sim` cannot
drift apart silently. Known exceptions per the master-spec test matrix: the TF card
(`hal_storage_ext`) and Wi-Fi/BLE (`hal_net` radio paths) are not emulated in QEMU, so
their contract tests run HIL-only while the SIM build provides deterministic stubs.
Every SIM driver additionally logs deterministic markers that the tests expect.

## 6. Build targets

| Target | Backend / Kconfig | Build | Run / test |
| --- | --- | --- | --- |
| Host | none (pure `core_*`) | `cmake -S firmware-main/host_test -B build-host -DSANITIZE=ON && cmake --build build-host` | `ctest --test-dir build-host --output-on-failure` |
| QEMU | `PD_HAL_SIM=y`, `sdkconfig.qemu` | `idf.py -B build_qemu -DSDKCONFIG=build_qemu/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.qemu" build` | `idf.py -B build_qemu qemu monitor`; `idf.py -B build_qemu qemu --graphics monitor`; `idf.py -B build_qemu qemu gdb`; `idf.py -B build_qemu qemu efuse-summary`; `pytest -m qemu` (pytest-embedded 2.9.3, all subpackages pinned) |
| Real board | `PD_HAL_REAL=y` (default) | `idf.py -B build build` (uses `sdkconfig.defaults` + `sdkconfig.defaults.esp32s3`) | `idf.py -B build flash monitor`; `pytest -m hil` on the self-hosted `hil-4.3b` runner |
| Release build | `sdkconfig.release` | `idf.py -B build_release -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.release" build` | unsigned images built in CI; signing happens only in the `firmware-signing` environment |

QEMU setup (one-time): `python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
qemu-riscv32`. ESP-IDF is pinned to v5.5.6 in blocking CI; the nightly workflow runs a
v6.1-rc1 canary leg that must not block merges.

## 7. Flash partition layout (16 MB)

`firmware-main/partitions.csv`. The bootloader resides at `0x0000` and the partition
table at `0x8000`; the named partitions follow.

| Name | Offset | Size | End | Purpose |
| --- | --- | --- | --- | --- |
| (bootloader) | `0x0000` | `0x8000` | `0x8000` | Second-stage bootloader |
| (partition table) | `0x8000` | — | `0x9000` | CSV-generated table |
| `nvs` | `0x9000` | `0x4000` | `0xd000` | Config, journal write-ahead state |
| `otadata` | `0xd000` | `0x2000` | `0xf000` | Selects `ota_0` / `ota_1` |
| `phy_init` | `0xf000` | `0x1000` | `0x10000` | Radio PHY calibration data |
| `ota_0` | `0x10000` | `0x640000` | `0x650000` | App slot A (rollback capable) |
| `ota_1` | `0x650000` | `0x640000` | `0xc90000` | App slot B |
| `littlefs` | `0xc90000` | `0x360000` | `0xff0000` | Journal, scenarios, UI assets |
| `coredump` | `0xff0000` | `0x10000` | `0x1000000` | Post-mortem core dumps |

## 8. Versioning flow

1. Contributors follow Conventional Commits (enforced by the `pr-title` workflow).
2. `release-please` (manifest mode) opens/updates release PRs and writes the bumped
   version to `firmware-main/version.txt`.
3. ESP-IDF reads `version.txt` as `PROJECT_VER` at build time.
4. `PROJECT_VER` is displayed on the device About screen (`ui`) and included in MQTT
   telemetry (`comms`), so support can always map a device to an exact release.

## 9. Monorepo map

| Path | Contents | Primary workflows |
| --- | --- | --- |
| `firmware-main/` | ESP32-S3 firmware: `components/`, `main/`, `host_test/`, `test/`, partitions, sdkconfigs | `ci-firmware-main` |
| `firmware-coproc/` | Co-processor firmware; shares the `core_protocol` codec sources | `ci-coproc` |
| `cloud/` | Firebase functions, bridge worker, `firestore.rules`, broker config | `ci-cloud`, `deploy-cloud` |
| `android-app/` | Caregiver/patient Android app | `ci-android` |
| `web-flasher/` | GitHub Pages browser flasher (ESP Web Tools) | `ci-web-flasher`, `pages` |
| `tools/` | CI checks (eFuse guard, sdkconfig release check, size diff), flasher manifest/merge generators, QEMU wrapper | invoked by the workflows above |
| `hardware/` | Schematics, mechanical, test jig | – |
| `docs/` | This documentation set | `ci-docs` |

Repository-level workflows: `codeql`, `dependency-review`, `scorecard`, `release-please`,
`release` (environment `firmware-signing`), `hil` (self-hosted, gated by the
`ENABLE_HIL` repository variable), `nightly`, `pr-title`.

## 10. Related documents

- `docs/protocol.md` — RS485 co-processor link protocol (frame format, CRC, FSM events).
- `docs/fmea.md` — failure mode and effects analysis for dose-safety risks.
- `docs/test-plan.md` — host / QEMU / HIL test matrix and suites.
- `docs/security-keys.md` — key management, secure boot v2 and flash encryption ceremony.
- `docs/master-spec.txt` — the source specification this document implements.

## 11. Decisions deferred

| Decision | Options under consideration | Target date |
| --- | --- | --- |
| Co-processor MCU | RP2040 / ESP32-C3 / STM32G0 | end of Phase 2, before `ci-coproc` is finalized |
| LVGL version pin | LVGL 8.x LTS vs 9.x | Phase 0.5 spike (b): `esp_lcd_qemu_rgb` + LVGL at 800x480 |
| LittleFS vs FATFS vs NVS | Dose journal, slot config and key-dedup persist in **NVS** (atomic small writes, no filesystem dependency); the `littlefs` partition stays reserved for bulk history/logs in Phase 6 | **decided for Phase 2**; revisit for bulk storage in Phase 6 |
