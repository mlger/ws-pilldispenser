#!/usr/bin/env python3
"""Validate firmware-main/sdkconfig.release (spec G3).

Checks: secure boot v2 on, flash encryption release mode on, PD_HAL_REAL=y,
PD_SIM_CONSOLE=n. Exits 1 on any violation.

Usage: check_release_sdkconfig.py [path-to-sdkconfig.release]
"""

from __future__ import annotations

import sys
from pathlib import Path

REQUIRED = [
    ("CONFIG_SECURE_BOOT=y", "secure boot v2 must be enabled"),
    ("CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y", "release images must be signed at build/sign time"),
    ("CONFIG_FLASH_ENCRYPTION_ENABLED=y", "flash encryption must be enabled"),
    ("CONFIG_FLASH_ENCRYPTION_MODE_RELEASE=y", "flash encryption must be RELEASE mode"),
    ("CONFIG_PD_HAL_REAL=y", "release builds link the real HAL backend"),
    ("CONFIG_LOG_DEFAULT_LEVEL_WARN=y", "release logs WARN"),
]

FORBIDDEN = [
    ("CONFIG_PD_SIM_CONSOLE=y", "sim console must never be enabled on release builds"),
    ("CONFIG_PD_HAL_SIM=y", "sim backend must never be linked on release builds"),
    ("CONFIG_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y", "FE dev mode forbidden in release sdkconfig"),
]


def main(argv: list[str]) -> int:
    path = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parents[2] / "firmware-main" / "sdkconfig.release"
    text = path.read_text(encoding="utf-8")

    failures: list[str] = []
    for line, reason in REQUIRED:
        if line not in text:
            failures.append(f"missing {line}  ({reason})")

    for line, reason in FORBIDDEN:
        if line in text:
            failures.append(f"forbidden {line}  ({reason})")

    if failures:
        print(f"check_release_sdkconfig: FAIL for {path}")
        for f in failures:
            print(f"  - {f}")
        return 1

    print(f"check_release_sdkconfig: OK ({path})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
