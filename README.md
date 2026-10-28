# PillDispenser Monorepo

Automated pill dispensing device: **Waveshare ESP32-S3-Touch-LCD-4.3B** main unit + RS485
co-processor mechanism + cloud bridge + Android companion app, engineered for
**dose-safety** and **testability-first** delivery.

Every line of business logic runs in three places with **zero `#ifdef`s** in application code:

| Target | Backend | Test driver |
| --- | --- | --- |
| Host (Linux) | pure C11, no ESP-IDF headers | CTest + ASan/UBSan (`firmware-main/host_test`) |
| ESP32-S3 under Espressif QEMU | `hal_sim` (`CONFIG_PD_HAL_SIM=y`) | pytest-embedded (`firmware-main/test`, marker `qemu`) |
| Real Waveshare 4.3B board | `hal_real` (`CONFIG_PD_HAL_REAL=y`) | HIL suite on self-hosted runner (marker `hil`) |

The backend is selected at build time by the Kconfig choice `PD_HAL_BACKEND`
(`firmware-main/components/pd_hal/Kconfig`). Deterministic test markers (e.g. `PD_READY`,
`FSM: <state>`) are gated behind `CONFIG_PD_TEST_HOOKS`.

## Repository layout

```text
/                       Monorepo root (release-please manifest mode)
├── firmware-main/      ESP32-S3 application (ESP-IDF v5.5.6, pinned)
├── firmware-coproc/    Dispense-mechanism co-processor (MCU decision deferred, Phase 3)
├── cloud/              Firebase functions + bridge worker + firestore.rules + broker config
├── android-app/        Companion app (Phase 5)
├── web-flasher/        Browser-based installer via ESP Web Tools on GitHub Pages
├── tools/              CI checks, QEMU wrapper, flasher manifest generation, signing
├── hardware/           Mechanical/electrical notes and test-jig documentation
└── docs/               Design docs (start with docs/architecture.md)
```

## Quickstart

```bash
# Host unit tests (no ESP-IDF required)
cmake -S firmware-main/host_test -B build-host -DSANITIZE=ON
cmake --build build-host -j
ctest --test-dir build-host --output-on-failure

# QEMU (requires ESP-IDF v5.5.6, see docs/setup-guide.md)
cd firmware-main && ../tools/qemu/run_qemu.sh

# Full automated QEMU suite
pytest firmware-main/test -m qemu \
  --embedded-services idf,qemu --target esp32s3 \
  --junitxml=reports/qemu.xml
```

Flashing the real board and the browser flasher are covered in
[docs/setup-guide.md](docs/setup-guide.md) and [docs/web-flasher.md](docs/web-flasher.md).

## Documentation

| Doc | Contents |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Hardware map, layering, components, partitions |
| [docs/protocol.md](docs/protocol.md) | RS485 PD-Link v1 frame format, FSM interaction |
| [docs/fmea.md](docs/fmea.md) | Dose-safety failure mode analysis |
| [docs/test-plan.md](docs/test-plan.md) | Host/QEMU/HIL execution matrix |
| [docs/qemu-testing.md](docs/qemu-testing.md) | QEMU install, build, automated suites |
| [docs/setup-guide.md](docs/setup-guide.md) | Developer environment setup |
| [docs/ci-cd.md](docs/ci-cd.md) | Workflows, pinning, branch protection, repo switches |
| [docs/release-process.md](docs/release-process.md) | release-please, signing, channels |
| [docs/security-keys.md](docs/security-keys.md) | Key handling, eFuse rules, provisioning ceremony |
| [docs/web-flasher.md](docs/web-flasher.md) | ESP Web Tools manifest contract, browser support |

## Phased delivery

| Phase | Scope | Status |
| --- | --- | --- |
| 0 | Design docs (this `docs/` set) | done |
| 0.5 | Repo scaffolding, CI green, host tests, QEMU smoke, placeholder flasher | **this drop** |
| 1 | Board bring-up (`hal_real` + drivers) + HAL contract tests | **this drop** (HIL runner lands with the bench) |
| 2 | Scheduler + FSM + offline storage | pending |
| 3 | Co-processor + mechanism | pending |
| 4 | Cloud (rules, bridge, MQTT) | pending |
| 5 | Android MVP | pending |
| 6 | History/refill/inventory/OTA + first tagged release | pending |
| 7 | Hardening (secure boot v2, flash encryption, Scorecard) | pending |

## Governance

- Conventional Commits enforced on PR titles (`pr-title` workflow); release-please cut releases.
- All GitHub Actions are pinned to full 40-char SHAs (see [docs/ci-cd.md](docs/ci-cd.md)).
- CI/HIL never burn eFuses: enforced by `tools/ci/check_no_efuse_burn.sh`.
- License: TBD (dependency review denies GPL-3.0 for `android-app`).

See [docs/master-spec.txt](docs/master-spec.txt) for the full engineering specification.
