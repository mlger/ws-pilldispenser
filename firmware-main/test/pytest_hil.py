"""Hardware-in-the-loop tests for the Waveshare ESP32-S3-Touch-LCD-4.3B.

Runs on: hil. CI: hil.yml (self-hosted runner labeled hil-4.3b, dev builds
only, never fork PRs, gated by vars.ENABLE_HIL).
Scope (docs/test-plan.md D6): GT911 touch, CH422G outputs, PCF85063 drift,
RS485 link with the real co-processor, motor/sensor jig, Wi-Fi/BLE
provisioning, TF card, RGB timing.
"""

import pytest

try:
    from pytest_embedded_idf import IdfDut
except ImportError:  # pragma: no cover - older plugin layouts
    from pytest_embedded_idf.dut import IdfDut  # type: ignore[no-redef]


@pytest.mark.hil
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['dev'], indirect=True)
def test_hil_boot_smoke(dut: IdfDut) -> None:
    """Real board boots and reports the deterministic ready marker."""
    dut.expect_exact('PD_READY', timeout=60)
