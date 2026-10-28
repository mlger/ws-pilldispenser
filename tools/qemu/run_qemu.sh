#!/usr/bin/env bash
# Interactive QEMU loop with project defaults (spec D3).
# Usage: tools/qemu/run_qemu.sh [extra idf.py qemu args...]
#   tools/qemu/run_qemu.sh monitor
#   tools/qemu/run_qemu.sh --graphics monitor
#   tools/qemu/run_qemu.sh gdb
set -euo pipefail

cd "$(dirname "$0")/../firmware-main"

idf.py -B build_qemu \
    -DSDKCONFIG=build_qemu/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.qemu" \
    build

exec idf.py -B build_qemu \
    -DSDKCONFIG=build_qemu/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.qemu" \
    qemu "$@"
