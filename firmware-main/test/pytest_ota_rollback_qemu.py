"""OTA download + rollback tests under QEMU.

Runs on: qemu. CI: ci-firmware-main -> qemu-tests (Phase 6).
Suite shape per spec D4:
  1. Host HTTP server serves version+1 image; device updates and reboots.
  2. Self-test fails on purpose -> bootloader rolls back -> test expects the
     previous version marker.
  3. Self-test passes -> esp_ota_mark_app_valid_cancel_rollback.
Activated in Phase 6 together with the OTA component.
"""

import pytest

try:
    from pytest_embedded_idf import IdfDut
except ImportError:  # pragma: no cover - older plugin layouts
    from pytest_embedded_idf.dut import IdfDut  # type: ignore[no-redef]


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 6: requires the OTA component and ota_0/ota_1 layout in the test app')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_ota_rollback_on_failed_selftest(dut: IdfDut) -> None:
    """Rollback path: invalid self-test reverts to the previous OTA slot."""


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 6: requires the OTA component')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_ota_valid_selftest_confirms_image(dut: IdfDut) -> None:
    """Happy path: valid self-test cancels the pending rollback."""
