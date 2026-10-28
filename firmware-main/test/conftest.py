"""Shared pytest configuration for PillDispenser firmware tests.

Every test module declares where it runs via markers (docs/test-plan.md):
- qemu: runs under Espressif QEMU (CI: ci-firmware-main -> qemu-tests)
- hil:  runs on real Waveshare 4.3B hardware (CI: hil.yml, self-hosted runner)
"""

import pytest


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line('markers', 'qemu: run under Espressif QEMU (esp32s3)')
    config.addinivalue_line(
        'markers', 'hil: run against real hardware (self-hosted hil-4.3b runner)'
    )
