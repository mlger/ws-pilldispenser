# Development Setup Guide

Status: Phase 0.5. Target audience: firmware developers and reviewers.

## Prerequisites

| Requirement | Version / notes |
| --- | --- |
| OS | Ubuntu 22.04 or 24.04 (x86_64) |
| git | any recent version |
| cmake | >= 3.16 (host tests) |
| gcc | host tests with ASan/UBSan |
| Python | >= 3.10 with pip (pytest-embedded 2.9.3) |
| ESP-IDF | v5.5.6, pinned, target esp32s3 |
| QEMU | installed via `idf_tools.py` (see below) |
| Optional | VS Code + the Espressif IDF extension |

## Install ESP-IDF v5.5.6

### Native install

```bash
mkdir -p ~/esp && cd ~/esp
git clone --recursive --branch v5.5.6 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

Source `./export.sh` in every new shell (or add an alias) before using `idf.py`.

### Docker alternative

CI uses the verified container image `espressif/idf:v5.5.6`; locally it behaves the same:

```bash
docker run --rm -it -v "$PWD":/project -w /project espressif/idf:v5.5.6
```

The image ships the ESP-IDF toolchain ready to use.

## Install QEMU

```bash
sudo apt-get install -y libgcrypt20 libglib2.0-0 libpixman-1-0 \
     libsdl2-2.0-0 libslirp0
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa qemu-riscv32
. $IDF_PATH/export.sh
```

Details, interactive usage and limitations: `docs/qemu-testing.md`.

## Host test quickstart

```bash
cmake -S firmware-main/host_test -B build-host -DSANITIZE=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Runs the `core_*` component tests on the host with ASan/UBSan; only gcc and cmake are needed.

## First QEMU run

```bash
tools/qemu/run_qemu.sh
```

The wrapper calls `idf.py qemu` with the project defaults (build dir `build_qemu`, `SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.qemu"`), builds if needed, boots the esp32s3 machine and attaches the IDF monitor. A healthy boot prints the deterministic test marker:

```text
PD_READY
```

The IDF monitor exits with Ctrl-]. If the console stays silent, see the troubleshooting table below.

## Flashing the Waveshare ESP32-S3-Touch-LCD-4.3B

- Use the UART Type-C port: the one wired to the board's USB-UART auto-download circuit. The other Type-C port is native USB (GPIO19/20) and will not flash firmware.
- The console runs on UART0 through the same UART port.
- If download mode is needed, hold BOOT while powering the board (or while plugging USB in).
- After flashing, press RESET. Download mode is not exited automatically after a forced boot.

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

- Linux: add your user to the `dialout` group, then log out and back in:

```bash
sudo usermod -aG dialout $USER
```

- macOS: if the serial port does not appear, install the USB-UART bridge driver.

## pytest-embedded (automated tests)

```bash
pip install -r firmware-main/test/requirements.txt
```

This pins `pytest-embedded`, `pytest-embedded-idf`, `pytest-embedded-qemu`, `pytest-embedded-serial` and `pytest-embedded-serial-esp` to `==2.9.3` (Python >= 3.10). Run the QEMU suites with:

```bash
pytest firmware-main/test -m qemu
```

The full command with services and reporter options is in `docs/qemu-testing.md`.

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Serial port does not appear | wrong port, missing driver, or board not in download mode | use the UART Type-C port (not native USB); on macOS install the USB-UART driver; hold BOOT while powering on |
| Permission denied on `/dev/ttyUSB0` | user not in `dialout` (Linux) | `sudo usermod -aG dialout $USER`, then re-login |
| Flashing fails or times out | serial monitor open, charge-only cable, board not in download mode | close all monitors, use a data-capable cable, hold BOOT while powering on, press RESET after flashing |
| QEMU boots silent | console routed to USB-Serial-JTAG, which is a stub in QEMU | rebuild with `sdkconfig.qemu` in the overlay chain (it forces console UART0) |
| QEMU flash size mismatch | custom image size differs from `CONFIG_ESPTOOLPY_FLASHSIZE` | the `--flash-file` image must be exactly 16 MB for this board |
| Test app never prints `PD_READY` | wrong overlay chain or wrong app path | use `tools/qemu/run_qemu.sh` or the exact build command from `docs/qemu-testing.md` |

## End users

If you only want to install firmware from a browser, no toolchain is required: see `docs/web-flasher.md`.
