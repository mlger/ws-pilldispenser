#!/usr/bin/env bash
# Merge ESP-IDF build parts into a single flashable image (spec E3).
# The merged .bin is flashed at address 0x0 (esptool / ESP Flash Download Tool,
# matching Waveshare's own procedure). NOTE: the merged image pads gaps with
# 0xFF and therefore must NOT be used for "update, keep settings" installs —
# the web flasher uses parts mode for that (docs/web-flasher.md).
set -euo pipefail

build_dir="${1:-.}"
cd "$build_dir"

# esptool v5 renamed merge_bin -> merge-bin; detect the spelling.
if python -m esptool version 2>/dev/null | grep -q "esptool.py v[5-9]"; then
    python -m esptool --chip esp32s3 merge-bin -o pd-main-merged.bin @flash_args
else
    python -m esptool --chip esp32s3 merge_bin -o pd-main-merged.bin @flash_args
fi

echo "merged image: $build_dir/pd-main-merged.bin (flash at 0x0)"
