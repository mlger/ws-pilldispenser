# PillDispenser Test Plan

Status: Phase 0 living document. Every test declares where it runs via pytest markers (qemu/hil) or CTest labels.

The same application code runs in three places with no `#ifdefs`: the host (pure C), an ESP32-S3 under Espressif QEMU, and the real Waveshare ESP32-S3-Touch-LCD-4.3B. Application code only includes `hal_*.h` interfaces; the Kconfig choice `PD_HAL_BACKEND` (`PD_HAL_REAL` / `PD_HAL_SIM`) selects which backend links. This plan states what is tested where, and which CI job enforces it.

## Test taxonomy

| Type | Scope | Where it runs | Selection |
| --- | --- | --- | --- |
| unit | Component Unity TEST_CASEs (scheduler, fsm, protocol codec, inventory, storage, comms, ota, coproc_link) | host (CTest) AND QEMU | CTest labels on host; `pytest -m qemu` against `test/apps/unit` |
| scenario | Full firmware built with `PD_HAL_SIM`, sim drivers and fault injection | QEMU | `pytest -m qemu` against `test/apps/scenario` |
| contract | Every HAL interface run against SIM (QEMU) and REAL (HIL) so the two backends cannot drift — `components/pd_hal/test/test_hal_contract.c`, linked into the unit app with both backends required so exactly the selected one links | QEMU + HIL | `pytest -m qemu` (SIM half), `pytest -m hil` (REAL half) |
| ui | LVGL renders and flows; framebuffer dumped to PNG, diffed against golden images (tolerance-based) | QEMU (headless in CI, `--graphics` locally) | `pytest -m qemu` |
| ota | OTA download, reboot, self-test fail leads to rollback; self-test pass marks the app valid | QEMU with a local HTTP server | `pytest -m qemu` (`test_ota_rollback_qemu.py`) |
| security | Flash encryption dev-mode boot on emulated eFuses; secure boot v2 case stays xfail | QEMU only, emulated eFuses | `pytest -m qemu` (`test_security_qemu.py`) |
| hil | Real hardware: GT911 touch, I2C parts, RS485 link, Wi-Fi/BLE provisioning, TF card, RGB timing | Nightly on the self-hosted runner `hil-4.3b` | `pytest -m hil` (`test_hil.py`) |

All pytest suites live in `firmware-main/test/` (`conftest.py` plus one `pytest_*` module per area and `fixtures/`); requirements are pinned to pytest-embedded `==2.9.3` (Python >= 3.10). Host tests live in `firmware-main/host_test/` (CMake + CTest with ASan/UBSan via `-DSANITIZE=ON`).

## Execution matrix

| Area | Host CTest | QEMU (esp32s3) | HIL |
| --- | --- | --- | --- |
| Scheduler, recurrence, DST/TZ math | yes | yes | smoke |
| Dispense FSM (incl. faults) | yes | yes (sim_coproc) | yes |
| RS485 protocol codec/CRC | yes | yes | yes (real link) |
| NVS / LittleFS persistence, power-loss replay | - | yes | yes |
| MQTT/TLS client logic, offline queue | - | yes (open_eth + host mosquitto) | yes (Wi-Fi) |
| LVGL screens (render, flows) | - | yes (--graphics / esp_lcd_qemu_rgb, screenshot diff) | yes |
| OTA download + rollback | - | yes (local HTTP server, reset) | yes |
| Flash encryption (dev mode) | - | yes (emulated eFuses only) | never burn in CI |
| Secure boot v2 | - | experimental on S3 (docs conflict) | manual ceremony only |
| GT911, CH422G, PCF85063 (I2C) | - | SIM drivers only | yes |
| Wi-Fi / BLE provisioning | - | no (not emulated) | yes |
| TF card (SD_CS via CH422G EXIO4) | - | no | yes |
| RGB LCD timing / ST7262 / tearing | - | no | yes |
| Motor, sensors, jam/empty detection | - | SIM only | yes |
| ADC (battery/sensors) | - | no (avoid calib at boot) | yes |

Legend: "-" means the area is not exercised in that environment. Host CTest covers only the pure-C `core_*` components (no IDF headers). A "no" in the QEMU column (Wi-Fi/BLE, TF card, RGB timing, ADC) marks what cannot be emulated and is therefore HIL-only; "SIM only" rows are validated against the real peripheral on HIL. "Never burn in CI" and "manual ceremony only" rows are hard eFuse rules enforced by `tools/ci/check_no_efuse_burn.sh`.

## Markers, labels and CI jobs

| CI job | What it runs | Notes |
| --- | --- | --- |
| lint | clang-format, cppcheck, `idf.py clang-check`, `tools/ci/check_no_efuse_burn.sh`, sdkconfig sanity | blocks PRs that would burn eFuses |
| host-tests | `cmake -S firmware-main/host_test -B build-host -DSANITIZE=ON` then `ctest --output-on-failure` | core_* components on the host |
| build (matrix) | `idf.py build` for legs dev / qemu / release-unsigned | artifacts: bins, ELF, map, flasher_args.json |
| size-report | `idf.py size --format json` + `tools/ci/size_diff.py` | fails if app partition free space < 15% or growth > 64 KB without label `size-ok` |
| qemu-tests | `pytest -m qemu` (services `idf,qemu`), needs the build[qemu] leg | unit, scenario, mqtt, ui, ota, security suites |
| hil (`hil.yml`) | `pytest -m hil --embedded-services esp,idf` | self-hosted runner labeled `hil-4.3b`, gated by repo variable `ENABLE_HIL`, dev builds only, never fork PRs |

A PR touching `firmware-main` cannot merge unless lint, host-tests, all build legs, size-report and qemu-tests pass. The nightly workflow adds the ESP-IDF canary leg (v6.1-rc1 docker image, non-blocking) and a long QEMU soak.

## Quality gates per phase

| Phase | Gate (test evidence required) |
| --- | --- |
| 0.5 | Hello-world app green through full ci-firmware-main including `pytest -m qemu` on esp32s3; host_test skeleton in place; spikes (a)-(f) have written reports; from here on every phase merges only through green CI |
| 1 | Board bring-up via hal_real (LCD, GT911, CH422G, PCF85063, RS485) with matching hal_sim drivers; HAL contract tests green against SIM (QEMU) and REAL (HIL); HIL runner online |
| 2 | Scheduler, FSM and offline storage: host CTest plus QEMU unit (61 cases) and scenario suites (DST edges EU/US incl. fall-back dedup, missed-dose catch-up, double-dose guard, jam/empty/no-ack faults, power-loss journal replay) are required checks — **Phase 2 drop: green** |
| 3 | Shared core_protocol codec tests identical on both sides of the RS485 link (ci-coproc); sim_coproc fault injection green in QEMU; HIL jig tests pass |
| 4 | Firestore rules emulator tests fully green; bridge integration vs mosquitto with the test CA; QEMU MQTT suite (TLS, LWT, retained state, reconnect backoff) required |
| 5 | Android MVP: ci-android green (ktlint, detekt, lint, unit tests); signed releases only in release.yml |
| 6 | QEMU OTA rollback suite green (rollback and valid-cancel-rollback paths); SBOM on the dev build; first release-please tag; web flasher stable/beta/dev channels live with checksums and changelog |
| 7 | Secure boot v2 + flash encryption release-mode ceremony executed manually and documented; signing via the `firmware-signing` environment; production units OTA-only; Scorecard; ESP-IDF v6.x migration only after the canary leg is green; FMEA review and penetration checklist |

## Test data and fixtures policy

- All fixtures live in `firmware-main/test/fixtures/`: `mosquitto.conf`, `gen_test_certs.sh` (generates a throwaway test CA), prebuilt OTA images and scenario files.
- The test CA and any host-generated flash-encryption keys are TEST-ONLY, marked as such in the fixture headers, used only by QEMU/CI tests, and regenerated if exposed. The normative key policy (production keys in KMS/HSM or environment secrets, never in the repo) is `docs/security-keys.md`.
- No real secrets and no real Firebase project IDs anywhere in fixtures or tests; use placeholders and GitHub `vars`/`secrets` names.
- eFuse safety: CI and HIL runners never burn real eFuses. eFuse operations are allowed only as `idf.py qemu efuse-*` or with `--qemu-efuse-path` inside tests; `tools/ci/check_no_efuse_burn.sh` enforces this on every PR.
- Device certificates are provisioned at the bench, never in CI; CI always uses the throwaway test CA.

## How to read a test run

- JUnit XML: QEMU legs write `reports/qemu.xml` (via `--junitxml=reports/qemu.xml`); the `qemu-tests` job publishes a test summary on the workflow run.
- Failure artifacts: the QEMU jobs upload JUnit XML, QEMU stdout and `qemu_flash.bin` on failure. Reproduce locally with `idf.py -B build_qemu qemu --flash-file <qemu_flash.bin> monitor` (the image is exactly 16 MB, matching `CONFIG_ESPTOOLPY_FLASHSIZE`).
- Host: `ctest --output-on-failure` prints the failing test; ASan/UBSan findings appear inline.
- HIL: logs and artifacts come from the nightly workflow on the self-hosted runner (`hil-4.3b`); a red nightly blocks release tags.

## References

- `docs/master-spec.txt` — Sections A (testability architecture) and D (testing plan)
- `docs/qemu-testing.md` — QEMU install, build, interactive usage, scenario authoring
- `docs/setup-guide.md` — developer environment from zero
- `docs/security-keys.md` — key custody, TEST-ONLY fixture rules
- `docs/spikes/phase0.5-spikes.md` — Phase 0.5 spike results (PSRAM, display, open_eth)
