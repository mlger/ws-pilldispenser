#!/usr/bin/env python3
"""Generate an ESP Web Tools manifest from an ESP-IDF build (spec E2).

Input:  build/flasher_args.json (never hand-write offsets).
Output: manifest.json with decimal offsets, chipFamily "ESP32-S3".

Includes otadata (ota_data_initial.bin) so the device boots ota_0 after a
browser install. Fails hard if the ESP32-S3 bootloader offset is not 0x0.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

# Logical id in flasher_args.json -> manifest part name.
PART_NAMES = {
    "bootloader": "bootloader.bin",
    "partition-table": "partition-table.bin",
    "otadata": "ota_data_initial.bin",
    "app": "app.bin",
}

EXPECTED_OFFSETS = {
    "bootloader": 0,          # ESP32-S3: 0x0 (never 0x1000)
    "partition-table": 0x8000,
    "otadata": 0xD000,
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True, help="ESP-IDF build dir with flasher_args.json")
    ap.add_argument("--name", default="PillDispenser Main (dev channel)")
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", default="manifest.json")
    ap.add_argument("--copy", action="store_true", help="copy part files next to the manifest")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    fargs_path = build_dir / "flasher_args.json"
    if not fargs_path.exists():
        print(f"ERROR: {fargs_path} not found — build the project first", file=sys.stderr)
        return 1

    fargs = json.loads(fargs_path.read_text(encoding="utf-8"))
    files: dict[str, str] = fargs.get("flash_files", {})
    offsets: dict[str, int] = fargs.get("flash_offsets", {})

    if offsets.get("bootloader") != EXPECTED_OFFSETS["bootloader"]:
        print(
            f"ERROR: bootloader offset is {offsets.get('bootloader')} but ESP32-S3 requires 0 "
            "(spec E2 fails a CI check on 4096)",
            file=sys.stderr,
        )
        return 1

    parts = []
    for logical in ("bootloader", "partition-table", "otadata", "app"):
        if logical not in files:
            if logical == "otadata":
                print(
                    "WARNING: otadata missing from flasher_args.json; synthesize "
                    "ota_data_initial.bin (8 KiB of 0xFF) so the device boots ota_0",
                    file=sys.stderr,
                )
                synth = build_dir / "ota_data_initial.bin"
                synth.write_bytes(b"\xff" * 0x2000)
                files["otadata"] = "ota_data_initial.bin"
                offsets["otadata"] = EXPECTED_OFFSETS["otadata"]
            else:
                print(f"ERROR: '{logical}' missing from flasher_args.json", file=sys.stderr)
                return 1

        src = build_dir / files[logical]
        if not src.exists():
            print(f"ERROR: part file {src} missing", file=sys.stderr)
            return 1

        name = PART_NAMES[logical]
        if args.copy:
            shutil.copy2(src, Path(args.out).parent / name)
        parts.append({"path": name, "offset": int(offsets[logical])})

    manifest = {
        "name": args.name,
        "version": args.version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": parts,
            }
        ],
    }

    Path(args.out).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"manifest written to {args.out}:")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
